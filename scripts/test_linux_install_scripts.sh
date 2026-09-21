#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLS="$ROOT/indra/newview/linux_tools"
FIXTURE=$(mktemp -d)
export FIXTURE
for script in install handle_secondlifeprotocol refresh_desktop_app_entry; do
    bash -n "$TOOLS/$script.sh"
done
# Load definitions only. Never run the real root/user installation entry point.
source <(sed '/^main "\$@"$/d' "$TOOLS/install.sh")
tarball_path="$FIXTURE/package"
mkdir -p "$tarball_path/etc"
printf '#!/bin/bash\nexit 0\n' > "$tarball_path/etc/refresh_desktop_app_entry.sh"
chmod +x "$tarball_path/etc/refresh_desktop_app_entry.sh"
printf 'payload' > "$tarball_path/payload"
RETAIN_BACKUPS=5
# Rejections must happen before any backup, pruning or mkdir.
for destination in "$tarball_path" "$tarball_path/child" "$FIXTURE" /; do
    if (backup_previous_installation() { exit 90; }; prune_old_backups() { exit 91; }; install_to_prefix "$destination") > "$FIXTURE/reject.log" 2>&1; then
        echo "Unexpected acceptance: $destination" >&2; exit 1
    else
        status=$?
        [[ $status == 1 ]] || { echo "Mutation before validation: $status"; exit 1; }
    fi
done
[[ -f "$tarball_path/payload" && ! -e "$tarball_path/child" ]]
# Similar path prefixes are not actual overlap; spaces and missing parents work.
install_to_prefix "$FIXTURE/package sibling/new install"
cmp "$tarball_path/payload" "$FIXTURE/package sibling/new install/payload"
# Resolve a destination symlink before any mutation.
ln -s "$tarball_path" "$FIXTURE/source-link"
if (install_to_prefix "$FIXTURE/source-link") > /dev/null 2>&1; then exit 1; fi
# Root suffix, without touching system directories or invoking a real registrar.
(
    DEFAULT_SYSTEM_INSTALL_DIR="$FIXTURE/system"
    SUFFIX=beta CUSTOM_INSTALL_DIR='' NON_INTERACTIVE=true
    mkdir -p "$FIXTURE/system_beta/etc"
    cp "$tarball_path/etc/refresh_desktop_app_entry.sh" "$FIXTURE/system_beta/etc/"
    install_to_prefix() { [[ "$1" == "$FIXTURE/system_beta" ]]; }
    mkdir() { :; }
    root_install
)
# A failed registrar must fail user and root installation entry points.
for entry in homedir_install root_install; do
    mkdir -p "$FIXTURE/fail/etc"
    printf '#!/bin/bash\nexit 7\n' > "$FIXTURE/fail/etc/refresh_desktop_app_entry.sh"
    chmod +x "$FIXTURE/fail/etc/refresh_desktop_app_entry.sh"
    if (CUSTOM_INSTALL_DIR="$FIXTURE/fail"; NON_INTERACTIVE=true; install_to_prefix() { :; }; mkdir() { :; }; "$entry") > /dev/null 2>&1; then exit 1; fi
done
source <(sed '/^if \[\[ "\$UID"/,$d' "$TOOLS/refresh_desktop_app_entry.sh")
install_desktop_entry "$FIXTURE/install with spaces" "$FIXTURE/menu"
grep -Fx "Exec=\"$FIXTURE/install with spaces/firestorm\"" "$FIXTURE/menu/vulkanstorm-viewer.desktop"
printf 'not a directory' > "$FIXTURE/blocked"
if install_desktop_entry "$FIXTURE/install" "$FIXTURE/blocked/menu" > /dev/null 2>&1; then exit 1; fi
# Both URL paths use fake executables and preserve the exact argument.
mkdir -p "$FIXTURE/viewer with spaces/etc" "$FIXTURE/commands"
cp "$TOOLS/handle_secondlifeprotocol.sh" "$FIXTURE/viewer with spaces/etc/"
printf '#!/bin/bash\nexit "${PIDOF_RESULT:-1}"\n' > "$FIXTURE/commands/pidof"
printf '#!/bin/bash\nprintf "%%s\\n" "$@" > "$FIXTURE/args"\n' > "$FIXTURE/commands/dbus-send"
cp "$FIXTURE/commands/dbus-send" "$FIXTURE/viewer with spaces/firestorm"
chmod +x "$FIXTURE/commands/"* "$FIXTURE/viewer with spaces/firestorm"
export PATH="$FIXTURE/commands:$PATH"
url='secondlife://Region Name/1/2/3?x=a&y=b'
PIDOF_RESULT=1 bash "$FIXTURE/viewer with spaces/etc/handle_secondlifeprotocol.sh" "$url"
mapfile -t args < "$FIXTURE/args"
[[ ${#args[@]} == 2 && ${args[0]} == -url && ${args[1]} == "$url" ]]
PIDOF_RESULT=0 bash "$FIXTURE/viewer with spaces/etc/handle_secondlifeprotocol.sh" "$url"
mapfile -t args < "$FIXTURE/args"
[[ ${args[4]} == "string:$url" ]]
echo "PASS: installer, desktop entry and URL handler fixtures ($FIXTURE)"
