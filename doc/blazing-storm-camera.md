# Blazing Storm camera control, first increment

Camera uses the existing `RemotePermission::Camera` bit. It is independent of
movement and is granted through the Camera tab or a trusted-controller profile.
There is no payment command or permission.

The existing command transport carries `camera-left`, `camera-right`,
`camera-up`, `camera-down`, `camera-in`, `camera-out`, and `camera-reset`.
Camera commands accept no target or text payload. The Subject dispatcher checks
the active session and sequence; RemoteActions checks Camera permission,
avatar validity, third-person mode, joystick flycam, and RLVa setcam/unlock locks.

Each controller button click requests one step: 0.1 radians for orbit/pitch or
0.25 metres for distance. Holding repeats steps at up to 10 Hz, without persistent
Subject-side held input. `LLAgentCamera::unlockView()` detaches the focus before
`cameraOrbitAround`, `cameraOrbitOver`, or `cameraOrbitIn`, avoiding their
avatar-yaw/pitch paths. Native camera distance constraints continue to apply.
There is no continuous input state or mouse interception. Focus accepts an
object/avatar UUID or the Controller's last world pick; the Subject resolves it
in its own object list and rejects unavailable objects and HUD attachments.

Reset, permission revocation, session replacement/end, emergency release, and
transport disconnect release camera ownership. `setFocusOnAvatar(true, false,
false)` restores avatar focus without changing avatar axes. Sessions that never
used camera control do not reset the camera. Normal local camera input remains
available. This restores ordinary focus, not a snapshot of the previous view.

## Verification

Release x64 viewer build passed for the interaction fixes and initial camera
implementation. XUI parses and all literal floater child bindings resolve.
A standalone C++ harness using the production coordinate parser accepted valid
coordinates and rejected NaN, infinity, overflow, trailing garbage, empty fields,
wrong field counts, and trailing separators.

Live two-viewer checks still required:

- Deny Camera while allowing Movement: camera commands must be rejected.
- Grant Camera without Movement: orbit/pitch/zoom must not rotate or move the avatar.
- Revoke Camera after orbiting; also test emergency release and peer disconnect.
  Avatar focus must return and subsequent commands must be rejected.
- Test RLVa setcam/unlock locks, distance limits, mouselook, and joystick flycam.
- Save/reload trusted Camera permission and verify old profiles remain ungranted.
- Sit on a child prim and touch a child prim: preserve the picked prim and offset;
  dialogs from the root and sibling scripts should mirror, unrelated dialogs should not.
- Touch locally as Subject; answer from either viewer, cancel locally, and wait
  for expiry. Verify only one response and closure of stale mirrors.
- Test empty/duplicate dialog labels, reconnect, and ScriptDialogs revocation.

The Camera panel scrolls at small window sizes and its buttons explicitly follow
the top edge, fixing the missing-button layout observed in the first increment.
