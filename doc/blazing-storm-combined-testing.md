# Combined Camera, Inventory, Teleport test build

Use `run-two-viewers.cmd` after building the Release viewer. Both viewers must
use this revision. Pair as usual, then grant permissions on the Subject's Camera,
Inventory, and Teleport tabs. Trusted profiles can store these grants separately.
Existing profiles gain no new permission automatically. Transport is still local
loopback; this change does not introduce an Internet relay.

## Included behavior

- Camera: click/hold orbit, pitch, distance, reset, and object/avatar focus using
  a UUID or the last Controller world pick. Local camera input remains available.
- Inventory: paged Subject inventory browser, folder navigation, clothing/body-part
  and attachment wear, outfit-folder add/remove, and copy-only object rez near the
  Subject. Body parts cannot be removed. Folder wear adds eligible items rather
  than replacing the entire current outfit. Gestures are not activated.
- Teleport: travel to the Controller, a current-grid location SLURL, offer/request
  teleport to/from an avatar UUID, and answer pending native teleport offers or
  requests. Maturity-change dialogs and administrator teleports remain local.
- No delete, move, rename, give, payment, purchase, or no-copy rez command exists.

The Subject validates current session permission, inventory membership, resolved
item type, copy permission, and RLVa restrictions at execution. A rez request
always sends RemoveItem=false. Links are resolved before checking copy rights.
Normal server parcel/rez and destination access rules still apply.

Snapshots carry at most 20 inventory entries and 20 pending teleport notices.
They refresh once per second. Closing/answering a native teleport notice removes
it from the Controller list; responses to closed or expired IDs are rejected.
Remote notice availability expires after two minutes. The Subject's native
notification is preserved. Session/transport reset clears all mirrored state.
Revocation clears browsing or pending-offer state and updates the Controller.

## One-pass live test checklist

1. Resize the floater to minimum size and a large size. All Camera buttons must
   remain visible/scrollable. Inventory and Teleport panels must scroll.
2. Pair with all new permissions off. Verify actions are disabled or rejected.
   Grant each permission independently; verify another permission grants no access.
3. Camera: click and hold each direction, zoom, focus an object and avatar, reset.
   Verify camera-only permission cannot turn the Subject avatar. Test RLVa camera
   locks and distance limits, mouselook, and flycam.
4. Inventory: browse root, folders, multiple pages, and an initially uncached folder.
   Wear/remove a clothing item and an attachment. Add/remove an outfit folder.
   If an outfit is still fetching, retry after fetch completes. Test locked items:
   eligible items may apply while locked items remain unchanged.
5. Rez a copyable object and verify the original stays in inventory. Try a no-copy
   object, a link to a no-copy object, a non-object item, and a trash item: none may
   rez. Test a no-rez parcel and RLVa rez/interact restrictions.
6. Teleport to Controller and a same-grid SLURL. Send an offer and a request to a
   test avatar. Have that avatar send offers/requests back; answer once on Subject
   and once on Controller, decline, and let a notice expire. No double response.
   Try an invalid SLURL, blocked destination, busy teleport, and RLVa TP locks.
7. Revoke each permission while its UI is open, then regrant it. Reconnect and use
   emergency release. No stale inventory view, old accepted notice, pending map
   lookup from a previous session, or continued camera movement should execute.
8. Save/reload a trusted profile; verify new permission values persist and old
   profiles remain ungranted. Repeat sit/touch/dialog and movement/chat smoke tests.

Request status means submitted to native processing, not confirmed by the server.
Rez failures and appearance/teleport restrictions may also appear in Subject UI.
Already-submitted native operations are not undone by ending possession.

## Automated validation

`blazingstorm/tests/bsremotevalidation_test.cpp` is a standalone C++17 test of the
production page/coordinate parsers and copy-only rez gate. Compile with MSVC from
a developer command prompt (without NDEBUG) and run the resulting executable.
Command-name round trips and unsupported payment/delete/give command rejection
were also checked against the production transport functions. XUI parses and all
literal floater bindings resolve; the new buttons have explicit top anchoring.

Live tests above require two logged-in avatars and have not been run automatically.
