# Linux crash reporting

Breakpad writes the minidump. The independent `linux-crash-logger.bin` process
asks for consent and uploads the report to the build's BugSplat database.
It uses the SDL3 library already shipped with the viewer. It does not require
Python, Tkinter, or a separately linked GTK/Qt/FLTK toolkit.

## User experience

With **Always ask before sending** enabled, the reporter opens immediately after
the crash and offers:

- **Send report**: submit the saved report to BugSplat.
- **Don't send**: mark the report as declined so it is not offered again.
- **Send later**: retain it for a later launch.
- **Report details**: show the version, captured name, attachment list, local
  report directory, and privacy-policy address before submitting.

The default and Escape actions are configured as **Send later**. Closing the
window also keeps the report without submitting it. A failed upload offers
another attempt or deferral.
The reporter records success only for an HTTP 2xx response; it retains failures.
**Always send** skips the dialog. **Never send** does not install the crash
handler or open pending reports.

Name and settings inclusion follow the existing crash-reporting preferences.
This first version has no editable fields or user-comment box. The viewer log
is not uploaded. A dump can contain personal information from memory; diagnostic
information and opted-in settings can also contain identifying information.

## Saved reports and recovery

During normal startup/login, the viewer saves system details and report defaults
in its per-run `dump-<uuid>/report-context.json`. The crash callback only starts
the reporter after Breakpad successfully creates the dump. The reporter snapshots
the files into `dump-<uuid>/report-<dump-id>/`, including:

- `minidump.dmp` and `CrashContext.json`;
- `debug_info.log` when available for the primary viewer instance;
- settings files only when the user has opted in;
- `report.json`, the local record of pending/sent/declined state;
- `bugsplat.log`, a timestamped submission log, which stays local.

The snapshots prevent a later session from overwriting the files for the crash.
Report directories have owner-only permissions and files have mode 0600.
Concurrent dialogs/uploads for the same report are locked, and completed or
declined reports are not retried. Files remain local after sending or declining;
they can be deleted manually from the report directory.

The next normal launch opens a retained pending report in the same independent
reporter window. To reopen a specific report manually:

```sh
/path/to/firestorm/bin/linux-crash-logger.bin --report /path/to/report.json
```

## Display support

SDL3 supplies its own X11 message boxes, including on XWayland. The bundled
SDL 3.2.24 uses the desktop's `zenity` command for native Wayland message boxes.
If no usable dialog backend is available, the reporter saves the report and logs
the error; it does not upload an **ask** report. There is no terminal fallback.
An available XWayland server is not assumed: the viewer removes `DISPLAY` when
using its native Wayland backend.

## Validation

With `LL_TESTS=ON`, `BUILD_TESTS` also builds the isolated crash-report driver.
CTest's `linux-crash-report` suite uses temporary files and a local HTTP server
to check consent failure, settings selection, snapshot stability, multipart
fields, upload errors/retries, concurrency, and duplicate prevention. The test
driver is not packaged and no test uploads to BugSplat.

Native Wayland and a real BugSplat submission still need a desktop smoke test.
An editable form and user comments remain a later step.
