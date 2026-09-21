"""Reject CI configuration whose viewer version differs from its source revision."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
revision = subprocess.check_output(
    ["git", "rev-list", "--count", "HEAD"], cwd=root, text=True
).strip()
base = (root / "indra/newview/VIEWER_VERSION_FS.txt").read_text().strip()
expected = f"{base}.{revision}"
actual = Path(sys.argv[1]).read_text().strip()
if (os.environ.get("revision") != revision or
        os.environ.get("AUTOBUILD_BUILD_ID") != revision or actual != expected):
    raise SystemExit(
        f"Build numbering mismatch: expected {expected}, got {actual}; "
        f"revision={os.environ.get('revision')}, "
        f"AUTOBUILD_BUILD_ID={os.environ.get('AUTOBUILD_BUILD_ID')}"
    )
print(f"Viewer version matches source history: {actual}")
