#!/bin/bash

SCRIPTSRC=`readlink -f "$0" || echo "$0"`
RUN_PATH=`dirname "${SCRIPTSRC}" || echo .`

install_prefix=${RUN_PATH}/..

# Keep this in sync with FIRESTORM_LAUNCHER_DESKTOP_ID in the Linux viewer
# build.  The official package uses firestorm-viewer.desktop; downstream
# packages may export a different valid storage ID before running this script.
desktop_id="${FIRESTORM_LAUNCHER_DESKTOP_ID:-firestorm-viewer.desktop}"
if [[ ! "${desktop_id}" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*\.desktop$ ]]; then
    echo "Invalid FIRESTORM_LAUNCHER_DESKTOP_ID '${desktop_id}'; using firestorm-viewer.desktop" >&2
    desktop_id="firestorm-viewer.desktop"
fi

function install_desktop_entry()
{
    local installation_prefix="$1"
    local desktop_entries_dir="$2"

    local desktop_entry="\
[Desktop Entry]\n\
Name=Firestorm Viewer\n\
Comment=Client for accessing 3D virtual worlds\n\
Exec=${installation_prefix}/firestorm\n\
Icon=${installation_prefix}/firestorm_icon.png\n\
Terminal=false\n\
Type=Application\n\
Categories=Application;Internet;Network;\n\
StartupNotify=true\n\
X-Desktop-File-Install-Version=3.0\n\
StartupWMClass=do-not-directly-run-firestorm-bin"

    echo " - Installing menu entries in ${desktop_entries_dir} (${desktop_id})"
    mkdir -vp "${desktop_entries_dir}"
    printf '%b\n' "${desktop_entry}" > "${desktop_entries_dir}/${desktop_id}" || echo "Failed to install application menu!" >&2
}

if [ "$UID" == "0" ]; then
    # system-wide
    install_desktop_entry "$install_prefix" /usr/local/share/applications
else
    # user-specific
    install_desktop_entry "$install_prefix" "$HOME/.local/share/applications"
fi
