"""Run path tests with temp directories selected before C++ initialization."""
# <FS:Beq> Extended non-ASCII path test coverage.
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable = str(Path(sys.argv[1]).resolve())
    result = 0
    with tempfile.TemporaryDirectory(prefix="na-") as temporary:
        for name in ("ASCII temp", "\u0416\u65e5\U0001f680 temp"):
            directory = Path(temporary, name)
            directory.mkdir()
            environment = os.environ.copy()
            environment.update({key: str(directory) for key in ("TEMP", "TMP", "TMPDIR")})
            environment["LL_TEST_EXPECTED_TEMP"] = str(directory)
            print(f"Temporary-directory scenario: {ascii(name)}", flush=True)
            completed = subprocess.run([executable, *sys.argv[2:]], env=environment,
                                       cwd=directory, timeout=45, check=False)
            if completed.returncode:
                result = 1
    return result


if __name__ == "__main__":
    sys.exit(main())
# </FS:Beq>
