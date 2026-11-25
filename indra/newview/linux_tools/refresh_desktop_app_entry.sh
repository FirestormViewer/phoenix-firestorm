#!/bin/bash

set -e # </FS:Beq> [FIRE-37144] Propagate registration errors to the installer.

SCRIPTSRC=`readlink -f "$0" || echo "$0"`
RUN_PATH=`dirname "${SCRIPTSRC}" || echo .`

install_prefix="$(realpath -- "${RUN_PATH}/..")"

function install_desktop_entry()
{
    local installation_prefix="$1"
    local desktop_entries_dir="$2"
    # <FS:Beq> [FIRE-37144] --yes accepts the default of keeping an existing entry.
    local filename=firestorm-viewer.desktop
    local destination="$desktop_entries_dir/$filename"
    local existing=false
    if [[ -L "$destination" || -f "$destination" ]]; then
        if [[ "$3" == --yes ]]; then
            echo "Keeping existing menu entry $destination"
            return 0
        fi
        existing=true
    elif [[ -e "$destination" ]]; then
        echo "Cannot install a menu entry over $destination (not a regular file)." >&2
        return 1
    fi

    # Keep ordinary paths byte-compatible with launchers from previous releases.
    # For special characters, Path sets the working directory. An absolute Exec
    # runs the executable wrapper there without interpreting the path as arguments.
    local exec_path="$installation_prefix/firestorm"
    if [[ "$exec_path" == *[!a-zA-Z0-9/._-]* ]]; then
        exec_path="/usr/bin/env ./firestorm"
    fi
    # Original template fields, now passed as printf arguments below.
    # Path=${installation_prefix}\n\
    # Exec=${installation_prefix}/firestorm\n\
    # Icon=${installation_prefix}/firestorm_icon.png\n\
    local desktop_entry="\
[Desktop Entry]\n\
Name=Firestorm Viewer\n\
GenericName=Firestorm Viewer\n\
Comment=Client for accessing 3D virtual worlds\n\
Path=%s\n\
Exec=%s\n\
Icon=%s/firestorm_icon.png\n\
Terminal=false\n\
Type=Application\n\
Categories=Game;Simulation;\n\
StartupNotify=true\n\
StartupWMClass="org.firestormviewer.firestorm"\n\
X-Desktop-File-Install-Version=3.0"
    # </FS:Beq>

    # <FS:Beq> [FIRE-37144] Compare the final entry, including desktop-file-install's version stamp.
    # echo " - Installing menu entries in ${desktop_entries_dir}"
    # WORK_DIR=`mktemp -d`
    # echo -e $desktop_entry > "${WORK_DIR}/firestorm-viewer.desktop" || "Failed to install application menu!"
    # desktop-file-install --dir="${desktop_entries_dir}" ${WORK_DIR}/firestorm-viewer.desktop
    # rm -r $WORK_DIR
    mkdir -p -- "$desktop_entries_dir"
    work_dir=$(mktemp -d -- "$desktop_entries_dir/.firestorm-XXXXXXXX")
    trap 'rm -rf -- "$work_dir"' EXIT

    # Path and Icon are Desktop Entry strings. Keep escapes in these fields
    # separate from the Exec command.
    local prefix_value=${installation_prefix//\\/\\\\}
    prefix_value=${prefix_value//$'\n'/\\n}
    prefix_value=${prefix_value//$'\t'/\\t}
    prefix_value=${prefix_value//$'\r'/\\r}
    printf "$desktop_entry\n" "$prefix_value" "$exec_path" "$prefix_value" > "$work_dir/$filename"
    desktop-file-install --dir="$work_dir/staged" "$work_dir/$filename"
    local candidate="$work_dir/staged/$filename"

    if [[ "$existing" == true ]]; then
        if [[ -f "$destination" ]] && cmp -s -- "$candidate" "$destination"; then
            return 0
        fi
        local answer
        printf 'Replace existing Firestorm menu entry %s? [y/N] ' "$destination"
        if ! read -r answer || [[ "$answer" != [yY] && "$answer" != [yY][eE][sS] ]]; then
            echo "Keeping existing menu entry $destination"
            return 0
        fi
        if [[ ! -L "$destination" && -f "$destination" ]]; then
            # Replace the contents but retain the user's rwx permissions.
            # The staged file is new; do not transfer special mode bits.
            local permissions
            permissions=$(stat -c '%03a' -- "$destination")
            chmod "${permissions: -3}" "$candidate"
        fi
        mv -fT -- "$candidate" "$destination"
    elif ! mv -nT -- "$candidate" "$destination" || [[ -e "$candidate" ]]; then
        # A concurrent install may have created the entry. GNU mv versions
        # differ in their exit status when --no-clobber skips a file.
        if [[ -f "$destination" || -L "$destination" ]]; then
            echo "Keeping existing menu entry $destination"
            return 0
        fi
        return 1
    fi
    echo "Installed menu entry $destination"
    # </FS:Beq>

    update-desktop-database "${desktop_entries_dir}"
}

if [ "$UID" == "0" ]; then
    # system-wide
    # <FS:Beq> [FIRE-37144] Forward the installer's non-interactive option.
    # install_desktop_entry "$install_prefix" /usr/local/share/applications
    install_desktop_entry "$install_prefix" /usr/local/share/applications "${1:-}"
    # </FS:Beq>
else
    # user-specific
    # <FS:Beq> [FIRE-37144] Only absolute XDG base directories are valid.
    # install_desktop_entry "$install_prefix" "$HOME/.local/share/applications"
    data_home=${XDG_DATA_HOME:-}
    [[ "$data_home" == /* ]] || data_home="$HOME/.local/share"
    install_desktop_entry "$install_prefix" "$data_home/applications" "${1:-}"
    # </FS:Beq>
fi
