"""Validate the configured release against the locally qualified feature set."""
import pathlib
import sys

cache = pathlib.Path(sys.argv[1])
windows = sys.argv[2] == "Windows"
values = {}
for line in cache.read_text(encoding="utf-8").splitlines():
    if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
        key, value = line.split("=", 1)
        values[key.split(":", 1)[0]] = value

enabled = {"PACKAGE", "USE_SOLOUD", "USE_AVX2_OPTIMIZATION", "USE_LTO",
           "USE_PRECOMPILED_HEADERS"}
disabled = {"OPENSIM", "USE_KDU", "INSTALL_PROPRIETARY", "USE_OPENAL",
            "USE_FMODSTUDIO", "USE_DISCORD", "USE_BUGSPLAT", "USE_TRACY",
            "USE_AVX_OPTIMIZATION", "USE_VELOPACK", "USE_NSIS", "LL_TESTS"}
(enabled if windows else disabled).update({"USE_INNOSETUP", "USE_MESAZINK"})
errors = []
for key in sorted(enabled | disabled):
    actual = values.get(key, "<missing>").upper()
    valid = {"ON", "TRUE", "1", "YES"} if key in enabled else {"OFF", "FALSE", "0", "NO"}
    if actual not in valid:
        errors.append(f"{key}: expected {'ON' if key in enabled else 'OFF'}, got {actual}")
for key, expected in {"CMAKE_BUILD_TYPE": "Release", "ADDRESS_SIZE": "64",
                      "VIEWER_CHANNEL": "Vulkanstorm-Release"}.items():
    if values.get(key) != expected:
        errors.append(f"{key}: expected {expected}, got {values.get(key)}")
if errors:
    raise SystemExit("Release configuration mismatch:\n" + "\n".join(errors))
print(f"Release configuration matches the local feature set ({sys.argv[2]}).")
