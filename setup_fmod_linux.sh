#!/usr/bin/env bash
# Packages the FMOD Studio 2.03.07 Linux API into the autobuild format
# expected by autobuild.xml, and places it at /opt/firestorm/.
#
# Usage:
#   ./setup_fmod_linux.sh /path/to/fmodstudioapi20307linux.tar.gz
#
# Download FMOD Studio 2.03.07 for Linux (free account required):
#   https://www.fmod.com/download  -> FMOD Studio API -> Linux -> 2.03.07

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AUTOBUILD_XML="$SCRIPT_DIR/autobuild.xml"
OUTPUT_DIR="/opt/firestorm"

# Detect version from filename (e.g. fmodstudioapi20312linux.tar.gz -> 2.03.12)
detect_fmod_version() {
    local fname
    fname=$(basename "$1")
    # Match patterns like api20307, api20312, etc.
    if [[ "$fname" =~ api([0-9]{1})([0-9]{2})([0-9]{2}) ]]; then
        echo "${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.${BASH_REMATCH[3]}"
    else
        echo ""
    fi
}

FMOD_VERSION=$(detect_fmod_version "$1")
if [[ -z "$FMOD_VERSION" ]]; then
    echo "ERROR: Could not detect FMOD version from filename: $(basename "$1")"
    echo "Expected a name like fmodstudioapi20312linux.tar.gz"
    exit 1
fi
OUTPUT_FILE="$OUTPUT_DIR/fmodstudio-${FMOD_VERSION}-linux64-251121739.tar.bz2"

# -----------------------------------------------------------------------
usage() {
    echo "Usage: $0 <path-to-fmodstudioapi20307linux.tar.gz>"
    echo ""
    echo "Download from: https://www.fmod.com/download"
    echo "  Product: FMOD Studio API  |  Platform: Linux  |  Version: 2.03.07"
}

if [[ $# -lt 1 ]]; then
    usage
    exit 1
fi

FMOD_ARCHIVE="$1"
if [[ ! -f "$FMOD_ARCHIVE" ]]; then
    echo "ERROR: File not found: $FMOD_ARCHIVE"
    exit 1
fi

# -----------------------------------------------------------------------
echo "=== FMOD Studio Linux autobuild package builder ==="
echo "Source : $FMOD_ARCHIVE"
echo "Output : $OUTPUT_FILE"
echo ""

# Create staging directories
STAGE=$(mktemp -d /tmp/fmod-stage-XXXXXX)
EXTRACT=$(mktemp -d /tmp/fmod-extract-XXXXXX)
trap 'rm -rf "$STAGE" "$EXTRACT"' EXIT

# Extract the FMOD archive
echo "Extracting FMOD archive..."
if file "$FMOD_ARCHIVE" | grep -qi "bzip2\|bz2"; then
    tar -xjf "$FMOD_ARCHIVE" -C "$EXTRACT"
elif file "$FMOD_ARCHIVE" | grep -qi "gzip\|gz"; then
    tar -xzf "$FMOD_ARCHIVE" -C "$EXTRACT"
elif file "$FMOD_ARCHIVE" | grep -qi "Zip"; then
    unzip -q "$FMOD_ARCHIVE" -d "$EXTRACT"
else
    echo "ERROR: Unsupported archive format. Expected .tar.gz, .tar.bz2, or .zip"
    exit 1
fi

# FMOD Studio API layout (Linux):
#   fmodstudioapi20307linux/api/core/inc/     -> headers
#   fmodstudioapi20307linux/api/core/lib/x86_64/   -> core .so files
#   fmodstudioapi20307linux/api/studio/inc/   -> studio headers
#   fmodstudioapi20307linux/api/studio/lib/x86_64/ -> studio .so files
FMOD_ROOT=$(find "$EXTRACT" -maxdepth 2 -name "api" -type d | head -n 1 | xargs dirname)
if [[ -z "$FMOD_ROOT" ]]; then
    echo "ERROR: Could not find 'api/' directory inside the archive."
    echo "Contents of archive root:"
    ls -la "$EXTRACT/"
    exit 1
fi
echo "Found FMOD root: $FMOD_ROOT"

CORE_INC="$FMOD_ROOT/api/core/inc"
CORE_LIB="$FMOD_ROOT/api/core/lib/x86_64"
STUDIO_INC="$FMOD_ROOT/api/studio/inc"
STUDIO_LIB="$FMOD_ROOT/api/studio/lib/x86_64"

for d in "$CORE_INC" "$CORE_LIB" "$STUDIO_INC" "$STUDIO_LIB"; do
    if [[ ! -d "$d" ]]; then
        echo "ERROR: Expected directory not found: $d"
        echo "This may be a different FMOD version. Please check the archive layout."
        exit 1
    fi
done

# Build the autobuild package layout:
#   include/fmodstudio/  -> all headers (cmake looks here: ${LIBS_PREBUILT_DIR}/include/fmodstudio)
#   lib/release/         -> .so files (versioned + unversioned symlinks)
#   LICENSES/            -> license file
mkdir -p "$STAGE/include/fmodstudio" "$STAGE/lib/release" "$STAGE/LICENSES"

echo "Copying headers..."
cp "$CORE_INC/"*.h   "$STAGE/include/fmodstudio/" 2>/dev/null || true
cp "$CORE_INC/"*.hpp "$STAGE/include/fmodstudio/" 2>/dev/null || true
cp "$STUDIO_INC/"*.h "$STAGE/include/fmodstudio/" 2>/dev/null || true
cp "$STUDIO_INC/"*.hpp "$STAGE/include/fmodstudio/" 2>/dev/null || true

echo "Copying libraries..."
cp "$CORE_LIB/"*.so*   "$STAGE/lib/release/" 2>/dev/null || true
cp "$STUDIO_LIB/"*.so* "$STAGE/lib/release/" 2>/dev/null || true

# Ensure unversioned symlinks exist (some FMOD zips omit them)
pushd "$STAGE/lib/release" > /dev/null
for versioned in libfmod.so.* libfmodstudio.so.*; do
    [[ -e "$versioned" ]] || continue
    base="${versioned%%.so.*}.so"
    [[ -e "$base" ]] || ln -s "$versioned" "$base"
done
popd > /dev/null

# License file
LICENSE_SRC=$(find "$FMOD_ROOT" -name "LICENSE.TXT" -o -name "license.txt" -o -name "fmod_license.txt" 2>/dev/null | head -n 1)
if [[ -n "$LICENSE_SRC" ]]; then
    cp "$LICENSE_SRC" "$STAGE/LICENSES/fmodstudio.txt"
else
    echo "FMOD Studio by Firelight Technologies Pty Ltd." > "$STAGE/LICENSES/fmodstudio.txt"
    echo "Note: License text not found in archive; placeholder written."
fi

# autobuild package metadata
# Top-level 'type' and 'version' are required by autobuild's configfile.py
# (__load validates: type == "metadata" and version == "1")
cat > "$STAGE/autobuild-package.xml" <<PKGXML
<?xml version="1.0" encoding="UTF-8"?>
<llsd>
  <map>
    <key>type</key>    <string>metadata</string>
    <key>version</key> <string>1</string>
    <key>package_description</key>
    <map>
      <key>copyright</key>     <string>FMOD Studio by Firelight Technologies Pty Ltd.</string>
      <key>description</key>   <string>FMOD Studio API</string>
      <key>license</key>       <string>fmod</string>
      <key>license_file</key>  <string>LICENSES/fmodstudio.txt</string>
      <key>name</key>          <string>fmodstudio</string>
      <key>version</key>       <string>${FMOD_VERSION}</string>
    </map>
  </map>
</llsd>
PKGXML

echo "FMOD version  : $FMOD_VERSION"

# Pack it up
# Pack it up — use --transform to strip leading ./ so archive members are
# named e.g. 'LICENSES/fmodstudio.txt' not './LICENSES/fmodstudio.txt',
# which is required for autobuild's tarfile.getmember() check.
echo "Creating tar.bz2..."
sudo mkdir -p "$OUTPUT_DIR"
(cd "$STAGE" && sudo tar -cjf "$OUTPUT_FILE" --transform 's,^\./,,' .)

# Compute MD5
MD5=$(md5sum "$OUTPUT_FILE" | awk '{print $1}')
echo ""
echo "Package created : $OUTPUT_FILE"
echo "MD5             : $MD5"

# Update autobuild.xml with the new hash
echo ""
echo "Updating autobuild.xml..."
python3 - "$AUTOBUILD_XML" "$MD5" <<'PYEOF'
import sys, re

xml_path, new_md5 = sys.argv[1], sys.argv[2]
with open(xml_path) as f:
    content = f.read()

# Find the linux64 fmodstudio block and replace its md5 hash.
# Pattern: inside the fmodstudio -> linux64 -> archive block.
pattern = (
    r'(fmodstudio-[0-9.]+-linux64[^<]*</string>\s*'
    r'</map>\s*<key>name</key>\s*<string>linux64</string>\s*</map>\s*'
    r'</map>\s*<key>name</key>\s*<string>linux64</string>\s*</map>\s*'
    r'<key>archive</key>\s*<map>\s*<key>hash</key>\s*<string>)[0-9a-f]+'
)
# Simpler targeted replacement using the known URL as anchor
old_block_re = re.compile(
    r'(<key>hash</key>\s*<string>)[0-9a-f]+(</string>\s*'
    r'<key>hash_algorithm</key>\s*<string>md5</string>\s*'
    r'<key>url</key>\s*<string>file:///opt/firestorm/fmodstudio[^<]*-linux64-[^<]*</string>)',
    re.DOTALL
)
new_content, n = old_block_re.subn(rf'\g<1>{new_md5}\2', content)
if n == 0:
    print("WARNING: Could not locate fmodstudio linux64 hash in autobuild.xml - update it manually.")
    print(f"  New MD5: {new_md5}")
    sys.exit(0)
with open(xml_path, 'w') as f:
    f.write(new_content)
print(f"autobuild.xml updated with MD5: {new_md5}")
PYEOF

echo ""
echo "=== Done! ==="
echo "Run build_linux.sh - it will detect /opt/firestorm/fmodstudio*.tar.bz2 and enable FMOD automatically."
