#!/bin/bash

SCRIPTSRC=$(readlink -f "$0") || exit 1
RUN_PATH=$(dirname "$SCRIPTSRC")
install_prefix=$(readlink -f "${RUN_PATH}/..") || exit 1

# Desktop-entry string escaping is applied after Exec argument quoting.
desktop_string() {
    local value="$1"
    value=${value//\\/\\\\}
    value=${value//$'\n'/\\n}
    value=${value//$'\r'/\\r}
    value=${value//$'\t'/\\t}
    printf '%s' "$value"
}

install_desktop_entry() {
    local installation_prefix="$1"
    local desktop_entries_dir="$2"
    local executable="${installation_prefix}/firestorm"
    executable=${executable//\\/\\\\}
    executable=${executable//\"/\\\"}
    executable=${executable//\$/\\\$}
    executable=${executable//\`/\\\`}
    executable=${executable//%/%%}

    echo " - Installing menu entries in ${desktop_entries_dir}"
    mkdir -p -- "$desktop_entries_dir" || return 1
    {
        printf '%s\n' '[Desktop Entry]' 'Name=Vulkanstorm Viewer' \
            'Comment=Client for accessing 3D virtual worlds'
        printf 'Exec=%s\n' "$(desktop_string "\"$executable\"")"
        printf 'Icon=%s\n' "$(desktop_string "${installation_prefix}/firestorm_icon.png")"
        printf '%s\n' 'Terminal=false' 'Type=Application' \
            'Categories=Internet;Network;' 'StartupNotify=true' \
            'StartupWMClass=do-not-directly-run-firestorm-bin'
    } > "${desktop_entries_dir}/vulkanstorm-viewer.desktop" || return 1
}

if [[ "$UID" == 0 ]]; then
    install_desktop_entry "$install_prefix" /usr/local/share/applications
else
    install_desktop_entry "$install_prefix" "${XDG_DATA_HOME:-$HOME/.local/share}/applications"
fi
