# Blazing Storm Relay

This service is the Internet relay for Blazing Storm possession sessions.

## Privacy model

The Subject and Controller never open a socket to each other. Both viewers make
an outbound WebSocket connection to this relay. The relay therefore knows each
client's network address, but it never sends either address to the other peer.

Deploy it behind HTTPS/WSS. Azure Container Apps, App Service, nginx, Caddy, or
another TLS-terminating reverse proxy are suitable.

The relay is intentionally permission-agnostic. It cannot grant viewer
permissions. The Subject viewer remains responsible for validating every remote
command.

## Handshake

Connect to:

`wss://YOUR-HOST/v1/relay`

The first WebSocket message must be JSON.

Create a session from the Subject:

```json
{"action":"create","role":"subject"}
```

The relay responds:

```json
{
  "type":"created",
  "sessionId":"...",
  "subjectToken":"...",
  "controllerToken":"...",
  "expiresAtUtc":"..."
}
```

The Subject should send only the session ID + Controller token to the intended
Controller, preferably inside the existing visible Second Life bootstrap IM.

The Controller then opens its own WebSocket and sends:

```json
{
  "action":"join",
  "role":"controller",
  "sessionId":"...",
  "token":"CONTROLLER_TOKEN"
}
```

When both sides are present each receives:

```json
{"type":"peer","state":"connected"}
```

After that, application messages are forwarded byte-for-byte to the other peer.
The server does not parse Blazing Storm command lines.

If either peer disconnects, the relay closes the other side and destroys the
session. This deliberately fails closed so the Subject viewer can end possession
immediately.

## Limits

Default limits:

- 128 KiB per WebSocket message
- 6 hour maximum session lifetime
- 10,000 in-memory sessions per relay instance

Sessions are currently in-memory. Use a single replica while testing. A later
multi-instance version can move session routing/state to Redis or Azure SignalR.

## Local test

```bash
dotnet run --project relay/BlazingStorm.Relay
```

Then connect to:

`ws://localhost:5000/v1/relay`

Production should use `wss://`, not plaintext WebSockets.

## Container

```bash
docker build -t blazing-storm-relay relay/BlazingStorm.Relay
docker run --rm -p 8080:8080 blazing-storm-relay
```

Health endpoint:

`GET /healthz`

## Next viewer step

The viewer still uses `LocalTransport` today. The next integration step is a
`RelayTransport` that carries the existing Blazing Storm line protocol over
WSS while reusing the same Subject-side dispatcher and permission model.
