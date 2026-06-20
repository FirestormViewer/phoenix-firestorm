#!/usr/bin/env bash
# Linux (Fedora) build script for Firestorm / Soapstorm Viewer
# Mirrors the logic of build.ps1 but targets Linux with cmake/make/ninja

set -euo pipefail

# ============================================================
# USER CONFIGURATION
# ============================================================
CLEAN=false
CONFIGURE=false
BUILD=false
ALL=false
CONFIG="ReleaseFS"
USE_CACHE=false
JOBS=0
CHANNEL="Release"
AVX2=true
PACKAGE=true
CREATE_RPM=true
OUTPUT_DIR=""

# ============================================================
# Argument parsing
# ============================================================
usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --clean               Remove build directory before building"
    echo "  --configure           Run configure step only"
    echo "  --build               Run build step only"
    echo "  --all                 Run both configure and build (default)"
    echo "  --config <name>       Autobuild config name (default: ReleaseFS)"
    echo "  --use-cache           Enable compiler cache (ccache/buildcache)"
    echo "  --jobs <n>            Parallel jobs (default: auto-detect CPU count)"
    echo "  --channel <name>      Viewer channel name (default: Release)"
    echo "  --no-avx2             Disable AVX2 optimisation"
    echo "  --no-package          Skip packaging step"
    echo "  --output-dir <path>   Copy build output to this directory"
    echo "  -h, --help            Show this help"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)        CLEAN=true ;;
        --configure)    CONFIGURE=true ;;
        --build)        BUILD=true ;;
        --all)          ALL=true ;;
        --config)       CONFIG="$2"; shift ;;
        --use-cache)    USE_CACHE=true ;;
        --jobs)         JOBS="$2"; shift ;;
        --channel)      CHANNEL="$2"; shift ;;
        --no-avx2)      AVX2=false ;;
        --no-package)   PACKAGE=false ;;
        --no-rpm)       CREATE_RPM=false ;;
        --output-dir)   OUTPUT_DIR="$2"; shift ;;
        -h|--help)      usage; exit 0 ;;
        *)              echo "Unknown option: $1"; usage; exit 1 ;;
    esac
    shift
done

# Default to All if neither --configure nor --build was given
if ! $CONFIGURE && ! $BUILD; then
    ALL=true
fi

# ============================================================
# Locate script's own directory (repo root)
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ============================================================
# Git branch detection
# ============================================================
GIT_BRANCH=""
if git_out=$(git rev-parse --abbrev-ref HEAD 2>/dev/null); then
    GIT_BRANCH="$git_out"
    echo "Current git branch: $GIT_BRANCH"
else
    echo "Warning: Not in a git repository or git not available"
fi

# ============================================================
# Build directory (Linux convention used by configure_firestorm.sh)
# ============================================================
BUILD_DIR="build-linux-x86_64"
echo ""
echo "Build directory: $BUILD_DIR"
if [[ -n "$GIT_BRANCH" && "$GIT_BRANCH" != "master" && "$GIT_BRANCH" != "main" ]]; then
    echo "Note: Building feature branch '$GIT_BRANCH' in shared build directory"
fi

# ============================================================
# Clean
# ============================================================
if $CLEAN; then
    echo ""
    echo "=== Cleaning build directory ==="
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        echo "Removed $BUILD_DIR"
    else
        echo "Build directory doesn't exist, nothing to clean"
    fi
fi

# ============================================================
# Configure
# ============================================================
if $CONFIGURE || $ALL; then
    echo ""
    echo "=== Configuring with autobuild ==="

    # AUTOBUILD_VARIABLES_FILE must point to the fs-build-variables file
    VARS_FILE="$(realpath "$SCRIPT_DIR/../fs-build-variables/variables")"
    if [[ ! -f "$VARS_FILE" ]]; then
        echo "ERROR: fs-build-variables not found at: $VARS_FILE"
        echo "Clone it alongside the repo: git clone https://vcs.firestormviewer.org/fs-build-variables ../fs-build-variables"
        exit 1
    fi
    export AUTOBUILD_VARIABLES_FILE="$VARS_FILE"
    export AUTOBUILD_ADDRSIZE="64"

    # Determine git revision for reproducible version numbers
    if GIT_REVISION=$(git rev-list --count HEAD 2>/dev/null | tr -d '[:space:]'); then
        if [[ "$GIT_REVISION" =~ ^[0-9]+$ ]]; then
            export VIEWER_REVISION="$GIT_REVISION"
            echo "Git revision: $GIT_REVISION"
        else
            export VIEWER_REVISION="0"
            echo "Warning: Could not determine git revision, using 0"
        fi
    else
        export VIEWER_REVISION="0"
    fi

    echo "AUTOBUILD_VARIABLES_FILE = $AUTOBUILD_VARIABLES_FILE"
    echo "AUTOBUILD_ADDRSIZE       = $AUTOBUILD_ADDRSIZE"
    echo "VIEWER_REVISION          = $VIEWER_REVISION"

    # ------------------------------------------------------------------
    # Auto-detect fmodstudio: include it only when the prebuilt package
    # exists at the path configured in autobuild.xml.
    # ------------------------------------------------------------------
    FMOD_PACKAGE=$(find /opt/firestorm -maxdepth 1 -name "fmodstudio-*-linux64-*.tar.bz2" 2>/dev/null | sort -r | head -n 1)
    if [[ -n "$FMOD_PACKAGE" && -f "$FMOD_PACKAGE" ]]; then
        FMOD_ENABLED=true
        echo "FMOD Studio package found: $FMOD_PACKAGE"
    else
        FMOD_ENABLED=false
        echo "FMOD Studio package not found at $FMOD_PACKAGE - building without audio"
        echo "  Run ./setup_fmod_linux.sh to set it up"
    fi

    # ------------------------------------------------------------------
    # Parse autobuild.xml to build the installable package list,
    # excluding proprietary / unavailable packages.
    # ------------------------------------------------------------------
    # Always exclude these; conditionally exclude fmodstudio
    # gntp-growl: linux64 archive is actually libnotify; use system package instead (sudo dnf install libnotify)
    # llappearance_utility: hosted on a private Linden Lab S3 bucket, not publicly accessible
    # libndofdev: no linux64 entry in autobuild.xml
    EXCLUDED_PACKAGES=("gntp-growl" "llappearance_utility" "libndofdev" "bugsplat" "havok-source" "kdu" "llphysicsextensions_source" "llphysicsextensions_tpv" "discord-rpc" "discord_sdk")
    if ! $FMOD_ENABLED; then
        EXCLUDED_PACKAGES+=("fmodstudio")
    fi

    # Use Python (available on all modern Fedora) to parse the LLSD XML
    EXCLUDED_SET=$(IFS=' '; echo "${EXCLUDED_PACKAGES[*]}")
    PACKAGE_LIST=$(python3 - $EXCLUDED_SET <<'PYEOF'
import xml.etree.ElementTree as ET, sys

excluded = set(sys.argv[1:])

tree = ET.parse("autobuild.xml")
root = tree.getroot()

# autobuild.xml: <llsd><map>...<key>installables</key><map>...</map>...
top_map = root.find("map")
if top_map is None:
    print("ERROR: Unexpected autobuild.xml structure", file=sys.stderr)
    sys.exit(1)

children = list(top_map)
for i, node in enumerate(children):
    if node.tag == "key" and node.text == "installables":
        install_map = children[i + 1]
        keys = [c.text for c in install_map if c.tag == "key" and c.text not in excluded]
        print("\n".join(keys))
        sys.exit(0)

print("ERROR: 'installables' key not found in autobuild.xml", file=sys.stderr)
sys.exit(1)
PYEOF
    )

    if [[ -z "$PACKAGE_LIST" ]]; then
        echo "ERROR: Failed to parse package list from autobuild.xml"
        exit 1
    fi

    EXCLUDED_STR=$(IFS=', '; echo "${EXCLUDED_PACKAGES[*]}")
    PKG_COUNT=$(echo "$PACKAGE_LIST" | wc -l)
    echo "Running: autobuild install (excluding: $EXCLUDED_STR)"
    echo "Installing $PKG_COUNT packages"

    # shellcheck disable=SC2086
    autobuild install --configuration "$CONFIG" $PACKAGE_LIST
    echo "autobuild install completed"

    # ------------------------------------------------------------------
    # Build the autobuild configure argument list
    # ------------------------------------------------------------------
    AUTOBUILD_ARGS=(
        "configure"
        "-c" "$CONFIG"
        "--"
    )

    if [[ -n "$CHANNEL" ]]; then
        AUTOBUILD_ARGS+=("--chan" "$CHANNEL")
    fi
    if $AVX2; then
        AUTOBUILD_ARGS+=("--avx2")
    fi
    if $PACKAGE; then
        AUTOBUILD_ARGS+=("--package")
    fi
    if $USE_CACHE; then
        AUTOBUILD_ARGS+=("--compiler-cache")
    fi
    if [[ "$JOBS" -gt 0 ]]; then
        AUTOBUILD_ARGS+=("--jobs" "$JOBS")
    fi

    if $FMOD_ENABLED; then
        AUTOBUILD_ARGS+=("--fmodstudio")
    fi

    AUTOBUILD_ARGS+=(
        "--platform" "linux"
        "-DLL_TESTS:BOOL=FALSE"
        "-DUSE_FMODSTUDIO:BOOL=$(if $FMOD_ENABLED; then echo TRUE; else echo FALSE; fi)"
        "-DHAVOK:BOOL=FALSE"
        "-DUSE_KDU:BOOL=FALSE"
        "-DOPENSIM:BOOL=FALSE"
    )

    echo "Running: autobuild ${AUTOBUILD_ARGS[*]}"
    autobuild "${AUTOBUILD_ARGS[@]}"

    echo "Configure completed successfully"
fi

# ============================================================
# RPM creation
# ============================================================
create_rpm() {
    echo ""
    echo "=== Creating RPM package ==="

    if ! command -v fpm &>/dev/null; then
        echo "Warning: 'fpm' not found — skipping RPM creation."
        echo "  Install with: sudo dnf install ruby ruby-devel gcc make && sudo gem install fpm"
        return 0
    fi

    # Locate the .tar.xz produced by the llpackage step
    local tarball
    tarball=$(find "$BUILD_DIR/newview" -maxdepth 1 -name "*.tar.xz" ! -name "*symbols*" | sort | head -n 1)
    if [[ -z "$tarball" ]]; then
        echo "Warning: No .tar.xz found in $BUILD_DIR/newview/ — skipping RPM creation."
        echo "  Make sure --package is set and the build completed fully."
        return 0
    fi
    tarball=$(realpath "$tarball")
    echo "Found tarball: $(basename "$tarball")"

    # Determine package version
    local pkg_version
    local build_data="$BUILD_DIR/newview/build_data.json"
    if [[ -f "$build_data" ]]; then
        pkg_version=$(python3 -c "import json; d=json.load(open('$build_data')); print(d['Version'])")
    else
        local base_ver git_rev
        base_ver=$(cat indra/newview/VIEWER_VERSION.txt)
        git_rev=$(git rev-list --count HEAD 2>/dev/null | tr -d '[:space:]' || echo "0")
        pkg_version="${base_ver}.${git_rev}"
    fi
    echo "Version: $pkg_version"

    # Stage payload into a temp dir laid out as the target filesystem
    local stage_dir
    stage_dir=$(mktemp -d /tmp/soapstorm-rpm-XXXXXX)
    # shellcheck disable=SC2064
    trap "rm -rf '$stage_dir'" RETURN

    local install_dir="$stage_dir/opt/soapstorm"
    mkdir -p "$install_dir"

    echo "Extracting viewer files..."
    # The tarball always has a single top-level versioned directory; strip it
    tar -xJf "$tarball" -C "$install_dir" --strip-components=1

    # The viewer package uses 'firestorm' as the wrapper script name.
    # Create a 'soapstorm' symlink so our launcher path /opt/soapstorm/soapstorm works.
    if [[ -f "$install_dir/firestorm" && ! -e "$install_dir/soapstorm" ]]; then
        ln -s firestorm "$install_dir/soapstorm"
    fi

    # /usr/bin wrapper script
    mkdir -p "$stage_dir/usr/bin"
    cat > "$stage_dir/usr/bin/soapstorm" <<'WRAPPER'
#!/bin/sh
exec /opt/soapstorm/soapstorm "$@"
WRAPPER
    chmod 755 "$stage_dir/usr/bin/soapstorm"

    # .desktop entry
    mkdir -p "$stage_dir/usr/share/applications"
    cat > "$stage_dir/usr/share/applications/soapstorm.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Soapstorm Viewer
Comment=Soapstorm Second Life Viewer
Exec=/opt/soapstorm/soapstorm
Icon=/opt/soapstorm/soapstorm_icon.png
Terminal=false
Type=Application
Categories=Network;
DESKTOP

    echo "Running fpm..."
    rm -f "${OUTPUT_DIR}/soapstorm-"*.rpm
    fpm \
        -s dir \
        -t rpm \
        --name "soapstorm" \
        --version "$pkg_version" \
        --architecture x86_64 \
        --description "Soapstorm Second Life Viewer" \
        --license "LGPL-2.1" \
        --url "https://github.com/FirestormViewer/phoenix-firestorm" \
        --no-auto-depends \
        --package "$SCRIPT_DIR" \
        --chdir "$stage_dir" \
        .

    local rpm_file
    rpm_file=$(find "$SCRIPT_DIR" -maxdepth 1 -name "soapstorm*.rpm" | sort -r | head -n 1)
    if [[ -n "$rpm_file" ]]; then
        echo ""
        echo "RPM created: $rpm_file"
    else
        echo "Warning: fpm completed but RPM not found in $SCRIPT_DIR"
    fi
}

# ============================================================
# Build
# ============================================================
if $BUILD || $ALL; then
    echo ""
    echo "=== Building ==="

    # If we skipped configure, patch build_data.json with the current commit
    # count so the package version string matches HEAD.
    if ! $CONFIGURE && ! $ALL; then
        BUILD_DATA_PATH="$BUILD_DIR/build_data.json"
        if [[ -f "$BUILD_DATA_PATH" ]]; then
            CURRENT_REVISION=$(git rev-list --count HEAD 2>/dev/null | tr -d '[:space:]')
            if [[ "$CURRENT_REVISION" =~ ^[0-9]+$ ]]; then
                python3 - "$BUILD_DATA_PATH" "$CURRENT_REVISION" <<'PYEOF'
import json, sys
path, rev = sys.argv[1], sys.argv[2]
with open(path) as f:
    data = json.load(f)
old_ver = data.get("Version", "")
parts = old_ver.split(".")
parts[-1] = rev
data["Version"] = ".".join(parts)
with open(path, "w") as f:
    json.dump(data, f, separators=(",", ":"))
print(f"Updated build_data.json: {old_ver} -> {data['Version']}")
PYEOF
            else
                echo "Warning: Could not determine git revision; build_data.json version not updated"
            fi
        fi
    fi

    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "ERROR: Build directory $BUILD_DIR doesn't exist. Run configure first."
        exit 1
    fi

    pushd "$BUILD_DIR" > /dev/null

    # Detect job count
    if [[ "$JOBS" -eq 0 ]]; then
        JOBS=$(nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo 4)
    fi

    # Prefer ninja if the build was configured with it; fall back to make
    if [[ -f "build.ninja" ]]; then
        echo "Running: ninja -j $JOBS"
        ninja -j "$JOBS"
    else
        echo "Running: make -j $JOBS"
        make -j "$JOBS"
    fi

    echo ""
    echo "Build completed successfully!"

    # Packaging step (cmake --build . --target package)
    if $PACKAGE; then
        echo ""
        echo "=== Running packaging step ==="
        if [[ -f "build.ninja" ]]; then
            ninja package || echo "Warning: Packaging completed with warnings"
        else
            make package || echo "Warning: Packaging completed with warnings"
        fi
        echo "Packaging completed"
    fi

    popd > /dev/null

    # Verify the executable was built
    EXE_PATH="$BUILD_DIR/newview/firestorm-bin"
    if [[ ! -f "$EXE_PATH" ]]; then
        echo "Warning: Expected executable not found at $EXE_PATH"
        echo "Check $BUILD_DIR/newview/ for the actual output"
    else
        echo "Executable: $EXE_PATH"
    fi

    # Copy to output directory if requested
    if [[ -n "$OUTPUT_DIR" ]]; then
        echo ""
        echo "=== Copying build to $OUTPUT_DIR ==="
        rm -rf "$OUTPUT_DIR"
        mkdir -p "$OUTPUT_DIR"
        cp -r "$BUILD_DIR/newview/." "$OUTPUT_DIR/"
        echo "Build copied to: $OUTPUT_DIR"
    fi

    # Create RPM from the packaged tarball
    if $CREATE_RPM && $PACKAGE; then
        create_rpm
    elif $CREATE_RPM && ! $PACKAGE; then
        echo ""
        echo "Note: Skipping RPM creation because --no-package was set (no .tar.xz to repack)"
    fi
fi

echo ""
echo "=== Build script completed ==="
