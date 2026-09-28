# Blazing Storm remote relay design

## Goal

Prevent the Controller and Subject from learning each other's public IP address.

The relay is not a remote-permission authority. Both viewers connect outward to
the relay and all commands still execute through the Subject-side permission and
validation code.

## Connection flow

1. Subject viewer opens a TLS WebSocket to the relay and creates an ephemeral
   relay session.
2. Relay returns a session ID and two random role tokens.
3. Subject keeps the Subject token private and sends the Controller token through
   the existing visible Second Life possession bootstrap IM.
4. Controller opens its own TLS WebSocket to the relay and joins with that token.
5. Relay announces that both peers are present.
6. Existing Blazing Storm protocol frames are carried through the relay.
7. Either peer disconnecting destroys the relay room and closes the other socket.

No peer address is included in any relay response or forwarded frame.

## Security decisions

- Production transport is WSS/TLS. Plain WS is development-only.
- Tokens are random 256-bit values and compared in constant time.
- Tokens are sent in the WebSocket handshake message, not the URL, to avoid
  leaking them through ordinary URL/access logs.
- The relay forwards application payloads and does not interpret permissions.
- Subject-side command validation remains mandatory.
- Sessions are ephemeral and in-memory for the first implementation.
- Any peer disconnect fails closed and ends the relay room.
- Money has no Blazing Storm permission or remote command and remains outside
  Full Control.

## Future hardening

Before calling the Internet transport production-ready:

- Add the viewer-side WSS `RelayTransport`.
- Pin/validate normal TLS certificates using Firestorm's supported trust path.
- Add application-level end-to-end encryption if relay operators should also be
  unable to read possession traffic.
- Add reconnect/resume only if it can preserve fail-closed semantics.
- Add rate limits and abuse controls around session creation.
- Use Redis/Azure SignalR (or sticky routing) before running multiple relay
  replicas.
- Add protocol version negotiation.
- Add automated relay integration tests using two WebSocket clients.
