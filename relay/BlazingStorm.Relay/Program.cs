using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Microsoft.AspNetCore.HttpOverrides;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddSingleton<RelayRegistry>();
builder.Services.Configure<RelayOptions>(builder.Configuration.GetSection("Relay"));
builder.Services.AddHostedService<RelayCleanupService>();

var app = builder.Build();

app.UseForwardedHeaders(new ForwardedHeadersOptions
{
    ForwardedHeaders = ForwardedHeaders.XForwardedFor | ForwardedHeaders.XForwardedProto
});

app.UseWebSockets(new WebSocketOptions
{
    KeepAliveInterval = TimeSpan.FromSeconds(20)
});

app.MapGet("/healthz", () => Results.Ok(new { status = "ok" }));

app.Map("/v1/relay", async (HttpContext context, RelayRegistry registry, CancellationToken stoppingToken) =>
{
    if (!context.WebSockets.IsWebSocketRequest)
    {
        context.Response.StatusCode = StatusCodes.Status400BadRequest;
        await context.Response.WriteAsync("WebSocket required.", stoppingToken);
        return;
    }

    using var socket = await context.WebSockets.AcceptWebSocketAsync();
    RelayRoom? room = null;
    RelayRole role = RelayRole.None;

    try
    {
        using var helloTimeout = CancellationTokenSource.CreateLinkedTokenSource(
            context.RequestAborted, stoppingToken);
        helloTimeout.CancelAfter(TimeSpan.FromSeconds(15));

        var helloPayload = await WebSocketHelpers.ReceiveMessageAsync(
            socket, 8 * 1024, helloTimeout.Token);

        if (helloPayload is null || helloPayload.Value.MessageType != WebSocketMessageType.Text)
        {
            await WebSocketHelpers.CloseQuietlyAsync(
                socket, WebSocketCloseStatus.PolicyViolation, "Handshake required.", stoppingToken);
            return;
        }

        RelayHello? hello;
        try
        {
            hello = JsonSerializer.Deserialize<RelayHello>(
                helloPayload.Value.Payload,
                RelayJson.Options);
        }
        catch (JsonException)
        {
            await WebSocketHelpers.CloseQuietlyAsync(
                socket, WebSocketCloseStatus.PolicyViolation, "Malformed handshake.", stoppingToken);
            return;
        }

        if (hello is null)
        {
            await WebSocketHelpers.CloseQuietlyAsync(
                socket, WebSocketCloseStatus.PolicyViolation, "Handshake required.", stoppingToken);
            return;
        }

        if (string.Equals(hello.Action, "create", StringComparison.OrdinalIgnoreCase))
        {
            if (!string.Equals(hello.Role, "subject", StringComparison.OrdinalIgnoreCase))
            {
                await WebSocketHelpers.CloseQuietlyAsync(
                    socket, WebSocketCloseStatus.PolicyViolation, "Only a subject can create a session.", stoppingToken);
                return;
            }

            room = registry.Create(socket);
            role = RelayRole.Subject;

            await room.SendControlAsync(
                RelayRole.Subject,
                new
                {
                    type = "created",
                    sessionId = room.SessionId,
                    subjectToken = room.SubjectToken,
                    controllerToken = room.ControllerToken,
                    expiresAtUtc = room.ExpiresAtUtc
                },
                stoppingToken);
        }
        else if (string.Equals(hello.Action, "join", StringComparison.OrdinalIgnoreCase))
        {
            role = RelayRoleParser.Parse(hello.Role);
            if (role == RelayRole.None
                || string.IsNullOrWhiteSpace(hello.SessionId)
                || string.IsNullOrWhiteSpace(hello.Token))
            {
                await WebSocketHelpers.CloseQuietlyAsync(
                    socket, WebSocketCloseStatus.PolicyViolation, "Invalid join request.", stoppingToken);
                return;
            }

            room = registry.Join(hello.SessionId, role, hello.Token, socket);
            if (room is null)
            {
                await WebSocketHelpers.CloseQuietlyAsync(
                    socket, WebSocketCloseStatus.PolicyViolation, "Session or token was invalid.", stoppingToken);
                return;
            }

            await room.NotifyPairStateAsync(stoppingToken);
        }
        else
        {
            await WebSocketHelpers.CloseQuietlyAsync(
                socket, WebSocketCloseStatus.PolicyViolation, "Unknown handshake action.", stoppingToken);
            return;
        }

        await room.RunPeerAsync(role, socket, context.RequestAborted);
    }
    catch (OperationCanceledException)
    {
        // Request, server shutdown, or handshake timeout.
    }
    catch (WebSocketException)
    {
        // The peer disappeared. Cleanup below closes the other side.
    }
    finally
    {
        if (room is not null)
        {
            await registry.EndSessionAsync(
                room.SessionId,
                role,
                "Relay peer disconnected.",
                CancellationToken.None);
        }
    }
});

app.Run();

enum RelayRole
{
    None,
    Subject,
    Controller
}

static class RelayRoleParser
{
    public static RelayRole Parse(string? value) =>
        value?.ToLowerInvariant() switch
        {
            "subject" => RelayRole.Subject,
            "controller" => RelayRole.Controller,
            _ => RelayRole.None
        };
}

sealed record RelayHello(
    string? Action,
    string? Role,
    string? SessionId,
    string? Token);

sealed class RelayOptions
{
    public int MaxMessageBytes { get; set; } = 128 * 1024;
    public int SessionLifetimeMinutes { get; set; } = 360;
    public int MaxSessions { get; set; } = 10_000;
}

sealed class RelayRegistry
{
    private readonly ConcurrentDictionary<string, RelayRoom> _rooms = new();
    private readonly RelayOptions _options;

    public RelayRegistry(Microsoft.Extensions.Options.IOptions<RelayOptions> options)
    {
        _options = options.Value;
    }

    public IEnumerable<RelayRoom> Rooms => _rooms.Values;

    public RelayRoom Create(WebSocket subjectSocket)
    {
        if (_rooms.Count >= _options.MaxSessions)
        {
            throw new InvalidOperationException("Relay session capacity reached.");
        }

        while (true)
        {
            var room = new RelayRoom(
                RandomId(18),
                RandomId(32),
                RandomId(32),
                subjectSocket,
                _options.MaxMessageBytes,
                DateTimeOffset.UtcNow.AddMinutes(_options.SessionLifetimeMinutes));

            if (_rooms.TryAdd(room.SessionId, room))
            {
                return room;
            }
        }
    }

    public RelayRoom? Join(
        string sessionId,
        RelayRole role,
        string token,
        WebSocket socket)
    {
        if (!_rooms.TryGetValue(sessionId, out var room)
            || room.ExpiresAtUtc <= DateTimeOffset.UtcNow
            || !room.TryJoin(role, token, socket))
        {
            return null;
        }

        return room;
    }

    public async Task EndSessionAsync(
        string sessionId,
        RelayRole disconnectedRole,
        string reason,
        CancellationToken cancellationToken)
    {
        if (_rooms.TryRemove(sessionId, out var room))
        {
            await room.CloseAsync(disconnectedRole, reason, cancellationToken);
        }
    }

    public async Task ExpireAsync(RelayRoom room, CancellationToken cancellationToken)
    {
        if (_rooms.TryRemove(room.SessionId, out var removed))
        {
            await removed.CloseAsync(
                RelayRole.None,
                "Relay session expired.",
                cancellationToken);
        }
    }

    private static string RandomId(int bytes)
    {
        return Convert.ToBase64String(RandomNumberGenerator.GetBytes(bytes))
            .TrimEnd('=')
            .Replace('+', '-')
            .Replace('/', '_');
    }
}

sealed class RelayRoom
{
    private readonly object _gate = new();
    private readonly int _maxMessageBytes;
    private PeerConnection? _subject;
    private PeerConnection? _controller;

    public RelayRoom(
        string sessionId,
        string subjectToken,
        string controllerToken,
        WebSocket subjectSocket,
        int maxMessageBytes,
        DateTimeOffset expiresAtUtc)
    {
        SessionId = sessionId;
        SubjectToken = subjectToken;
        ControllerToken = controllerToken;
        ExpiresAtUtc = expiresAtUtc;
        _maxMessageBytes = maxMessageBytes;
        _subject = new PeerConnection(subjectSocket);
    }

    public string SessionId { get; }
    public string SubjectToken { get; }
    public string ControllerToken { get; }
    public DateTimeOffset ExpiresAtUtc { get; }

    public bool TryJoin(RelayRole role, string token, WebSocket socket)
    {
        lock (_gate)
        {
            if (role == RelayRole.Subject)
            {
                if (!FixedEquals(token, SubjectToken) || _subject is not null)
                {
                    return false;
                }

                _subject = new PeerConnection(socket);
                return true;
            }

            if (role == RelayRole.Controller)
            {
                if (!FixedEquals(token, ControllerToken) || _controller is not null)
                {
                    return false;
                }

                _controller = new PeerConnection(socket);
                return true;
            }

            return false;
        }
    }

    public async Task RunPeerAsync(
        RelayRole role,
        WebSocket socket,
        CancellationToken cancellationToken)
    {
        while (socket.State == WebSocketState.Open
               && !cancellationToken.IsCancellationRequested)
        {
            var message = await WebSocketHelpers.ReceiveMessageAsync(
                socket,
                _maxMessageBytes,
                cancellationToken);

            if (message is null
                || message.Value.MessageType == WebSocketMessageType.Close)
            {
                return;
            }

            PeerConnection? destination;
            lock (_gate)
            {
                destination = role == RelayRole.Subject ? _controller : _subject;
            }

            if (destination is null
                || destination.Socket.State != WebSocketState.Open)
            {
                continue;
            }

            await destination.SendAsync(
                message.Value.Payload,
                message.Value.MessageType,
                cancellationToken);
        }
    }

    public async Task NotifyPairStateAsync(CancellationToken cancellationToken)
    {
        PeerConnection? subject;
        PeerConnection? controller;
        lock (_gate)
        {
            subject = _subject;
            controller = _controller;
        }

        if (subject is null || controller is null)
        {
            return;
        }

        var payload = JsonSerializer.SerializeToUtf8Bytes(
            new { type = "peer", state = "connected" },
            RelayJson.Options);

        await subject.SendAsync(payload, WebSocketMessageType.Text, cancellationToken);
        await controller.SendAsync(payload, WebSocketMessageType.Text, cancellationToken);
    }

    public async Task SendControlAsync(
        RelayRole role,
        object value,
        CancellationToken cancellationToken)
    {
        PeerConnection? peer;
        lock (_gate)
        {
            peer = role == RelayRole.Subject ? _subject : _controller;
        }

        if (peer is null)
        {
            return;
        }

        var payload = JsonSerializer.SerializeToUtf8Bytes(value, RelayJson.Options);
        await peer.SendAsync(payload, WebSocketMessageType.Text, cancellationToken);
    }

    public async Task CloseAsync(
        RelayRole disconnectedRole,
        string reason,
        CancellationToken cancellationToken)
    {
        PeerConnection? subject;
        PeerConnection? controller;

        lock (_gate)
        {
            subject = _subject;
            controller = _controller;
            _subject = null;
            _controller = null;
        }

        var tasks = new List<Task>(2);
        if (subject is not null && disconnectedRole != RelayRole.Subject)
        {
            tasks.Add(subject.CloseAsync(reason, cancellationToken));
        }
        if (controller is not null && disconnectedRole != RelayRole.Controller)
        {
            tasks.Add(controller.CloseAsync(reason, cancellationToken));
        }

        await Task.WhenAll(tasks);
    }

    private static bool FixedEquals(string left, string right)
    {
        var a = Encoding.UTF8.GetBytes(left);
        var b = Encoding.UTF8.GetBytes(right);
        return a.Length == b.Length
            && CryptographicOperations.FixedTimeEquals(a, b);
    }
}

sealed class PeerConnection
{
    private readonly SemaphoreSlim _sendLock = new(1, 1);

    public PeerConnection(WebSocket socket)
    {
        Socket = socket;
    }

    public WebSocket Socket { get; }

    public async Task SendAsync(
        ReadOnlyMemory<byte> payload,
        WebSocketMessageType messageType,
        CancellationToken cancellationToken)
    {
        await _sendLock.WaitAsync(cancellationToken);
        try
        {
            if (Socket.State == WebSocketState.Open)
            {
                await Socket.SendAsync(
                    payload,
                    messageType,
                    endOfMessage: true,
                    cancellationToken);
            }
        }
        finally
        {
            _sendLock.Release();
        }
    }

    public Task CloseAsync(string reason, CancellationToken cancellationToken) =>
        WebSocketHelpers.CloseQuietlyAsync(
            Socket,
            WebSocketCloseStatus.NormalClosure,
            reason,
            cancellationToken);
}

readonly record struct RelayMessage(
    byte[] Payload,
    WebSocketMessageType MessageType);

static class WebSocketHelpers
{
    public static async Task<RelayMessage?> ReceiveMessageAsync(
        WebSocket socket,
        int maxBytes,
        CancellationToken cancellationToken)
    {
        var buffer = new byte[Math.Min(16 * 1024, maxBytes)];
        using var stream = new MemoryStream();

        while (true)
        {
            var result = await socket.ReceiveAsync(buffer, cancellationToken);
            if (result.MessageType == WebSocketMessageType.Close)
            {
                return new RelayMessage(Array.Empty<byte>(), WebSocketMessageType.Close);
            }

            if (stream.Length + result.Count > maxBytes)
            {
                await CloseQuietlyAsync(
                    socket,
                    WebSocketCloseStatus.MessageTooBig,
                    "Relay message exceeded the size limit.",
                    cancellationToken);
                return null;
            }

            await stream.WriteAsync(buffer.AsMemory(0, result.Count), cancellationToken);
            if (result.EndOfMessage)
            {
                return new RelayMessage(stream.ToArray(), result.MessageType);
            }
        }
    }

    public static async Task CloseQuietlyAsync(
        WebSocket socket,
        WebSocketCloseStatus status,
        string description,
        CancellationToken cancellationToken)
    {
        if (socket.State is not (WebSocketState.Open or WebSocketState.CloseReceived))
        {
            return;
        }

        try
        {
            await socket.CloseAsync(status, description, cancellationToken);
        }
        catch
        {
            // Best-effort shutdown.
        }
    }
}

static class RelayJson
{
    public static readonly JsonSerializerOptions Options =
        new(JsonSerializerDefaults.Web)
        {
            PropertyNameCaseInsensitive = true
        };
}

sealed class RelayCleanupService : BackgroundService
{
    private readonly RelayRegistry _registry;

    public RelayCleanupService(RelayRegistry registry)
    {
        _registry = registry;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(TimeSpan.FromSeconds(30));

        while (await timer.WaitForNextTickAsync(stoppingToken))
        {
            var now = DateTimeOffset.UtcNow;
            foreach (var room in _registry.Rooms)
            {
                if (room.ExpiresAtUtc <= now)
                {
                    await _registry.ExpireAsync(room, stoppingToken);
                }
            }
        }
    }
}
