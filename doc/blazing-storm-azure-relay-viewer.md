# Blazing Storm viewer integration with Azure relay

This document describes the viewer-side transport on branch
`blazing/relay-webpubsub`.

It targets the currently deployed relay architecture:

- Azure Functions broker
- Node.js 22
- Azure Web PubSub
- hub: `blazing-remote`
- Controller -> Subject application channel: `commands`
- Subject -> Controller application channel: `events`

The old standalone `/v1/relay` ASP.NET WebSocket service documented elsewhere
is not the transport used by this branch.

## Viewer configuration

At the top of **Blazing Storm Remote Control**:

- **Relay broker**: the HTTPS base URL of the Azure Function App, for example
  `https://YOUR-FUNCTION.azurewebsites.net`.
- **Subject key**: the Relay Create Key. This is required only on a viewer/avatar
  that can become the Subject. It is intentionally not persisted to the normal
  per-account settings file.

Both viewers should configure the same trusted broker base URL.

The Controller never receives or needs the Relay Create Key.

If **Relay broker** is blank, the existing loopback-only transport remains
available for local debug testing.

The deployed broker health endpoint is expected at:

`GET /api/health`

with protocol version 1 and hub `blazing-remote`.

## Possession flow

1. Controller starts a normal Blazing Storm possession request.
2. The Controller viewer sends a visible Second Life IM containing its UUID and
   a fresh one-time nonce.
3. Subject validates that the actual IM sender matches the embedded UUID.
4. Trusted Controllers auto-approve. Other Controllers use the normal Subject
   Accept/Decline prompt.
5. Only after Subject approval, the Subject calls the Azure Function create
   endpoint using the locally configured Relay Create Key.
6. The broker returns a relay session ID plus distinct short-lived Subject and
   Controller tickets.
7. Subject sends only the session ID, Controller ticket, and original nonce to
   the Controller through a visible Second Life IM.
8. Subject exchanges its Subject ticket with the broker for a short-lived Azure
   Web PubSub client access URL.
9. Controller validates the Subject sender and nonce, then exchanges the
   Controller ticket for its own client access URL.
10. Each viewer opens an outbound WSS connection using the
    `json.webpubsub.azure.v1` subprotocol.
11. Existing Blazing Storm application frames are sent as Web PubSub text
    `sendToGroup` messages.
12. The Controller publishes to the commands direction and receives events.
    The Subject receives commands and publishes events.
13. The existing Blazing Storm `REQUEST|...` identity/nonce validation still
    runs after the relay is connected.
14. Existing Subject-side permission checks remain authoritative for every
    command.

The broker and Web PubSub do not grant possession permissions.

## Broker compatibility

The v0.2 server package's exact route strings were not available in this source
tree, so the viewer has conservative route discovery for create/negotiate
operations. It advances to another candidate only for HTTP 404/405. Authentication,
validation, rate-limit, and server errors stop immediately and are shown in the
Blazing Storm status.

If the deployed Function uses custom routes, these per-account debug settings
override discovery without changing C++ code:

- `BlazingStormRelayCreatePath`
- `BlazingStormRelaySubjectNegotiatePath`
- `BlazingStormRelayControllerNegotiatePath`

A negotiate override may include `{session}`, which is replaced with the
current relay session ID.

The client accepts common broker response aliases and also inspects the Azure
Web PubSub access-token claims to determine scoped send roles and initially
joined groups.

## Security properties

- Both viewers make outbound connections; neither opens an Internet listener.
- The Controller cannot choose the Subject's broker URL.
- Relay Create Key is Subject-only and is not included in URLs, SL IMs, or
  Web PubSub messages.
- The Relay Create Key field is runtime-only in the normal settings system.
- TLS certificate and hostname validation are enabled.
- On Windows, the viewer imports trusted Windows root certificates into the
  OpenSSL context used by the relay transport.
- Session and Controller tickets are short-lived bootstrap material, not
  possession permissions.
- The existing SL sender UUID + nonce checks remain in place.
- Money actions remain outside Blazing Storm remote permissions.
- Relay/network failure ends the Blazing Storm connection fail-closed.
- Existing application command sequence/replay checks and Subject-side
  permission validation still apply.

## First live test

1. Verify the deployed server health endpoint in a browser.
2. Build `blazing/relay-webpubsub` on both viewers.
3. Enter the same Function App HTTPS base URL in **Relay broker** on both.
4. On the Subject only, paste the Relay Create Key into **Subject key**.
5. Controller requests possession through the existing UI/IM button.
6. Subject accepts.
7. Status should progress through create, negotiate, Web PubSub connect, and
   finally `paired`.
8. Test one movement command first.
9. Then test chat, sit/touch, blue menus, camera, teleport, and inventory.
10. Disconnect one side and confirm the other side ends possession.
11. Start a second session to verify no stale relay session/ticket state remains.

If route discovery fails, copy the status line containing the missing route or
set the matching path override from the server's `VIEWER-INTEGRATION.md`.

## Current limitations

- No automatic Web PubSub reconnect/resume yet. A dropped relay session fails
  closed and a new possession request is required.
- The exact deployed Function route names should eventually be copied from the
  server package's `VIEWER-INTEGRATION.md` into this source so route discovery
  can be reduced to the canonical paths.
