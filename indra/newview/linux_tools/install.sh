#!/bin/bash

# Firestorm Viewer Installation Script

# ANSI color codes for styling
VT102_STYLE_NORMAL='\E[0m'
VT102_COLOR_RED='\E[31m'

# Resolve the script's directory
SCRIPTSRC=$(readlink -f "$0" || echo "$0")
RUN_PATH=$(dirname "${SCRIPTSRC}" || echo .)
tarball_path="${RUN_PATH}"

# Default installation directories
DEFAULT_SYSTEM_INSTALL_DIR="/opt/firestorm"
DEFAULT_USER_INSTALL_DIR="$HOME/firestorm"

# Default number of retained backups
DEFAULT_RETAIN_BACKUPS=5

# Function to display usage information
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --suffix <suffix>         Append a suffix to the installation directory (e.g., beta, release, debug)"
    echo "  --install-dir <directory> Specify a custom installation directory"
    echo "  --retain <count>          Number of backup versions to keep (default: ${DEFAULT_RETAIN_BACKUPS})"
    echo "  --yes, -y                 Non-interactive mode (accept defaults)"
    echo "  --help, -h                Display this help message"
    exit 1
}

# Function to display warning messages in red
warn() {
    echo -e "${VT102_COLOR_RED}$1${VT102_STYLE_NORMAL}"
}

# Function to exit the script with a warning message
die() {
    warn "$1"
    exit 1
}

# Function to create a timestamped backup of the existing installation
backup_previous_installation() {
    local install_dir="$1"
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_dir="${install_dir}-${timestamp}"

    echo " - Backing up previous installation from ${install_dir} to ${backup_dir}"
    mv "$install_dir" "$backup_dir" || die "Failed to create backup of existing installation!"
    echo " - Backup created successfully."
}

# Function to remove old backups beyond configured retention
prune_old_backups() {
    local install_dir="$1"
    local retain="$2"
    local backups=()
    local i
    local path

    shopt -s nullglob
    for path in "${install_dir}"-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]_[0-9][0-9][0-9][0-9][0-9][0-9]; do
        [[ -e "$path" ]] || continue
        backups+=("$path")
    done
    shopt -u nullglob

    if (( ${#backups[@]} > 0 )); then
        mapfile -t backups < <(printf '%s\n' "${backups[@]}" | sort -r)
    fi

    echo " - Backup retention: found ${#backups[@]} backups, retaining ${retain}"

    # Nothing to prune.
    if (( ${#backups[@]} <= retain )); then
        return
    fi

    for (( i=retain; i<${#backups[@]}; i++ )); do
        echo " - Removing old backup: ${backups[i]}"
        rm -rf -- "${backups[i]}" || die "Failed to remove old backup: ${backups[i]}"
    done
}

# Function to install Firestorm Viewer to the specified directory
install_to_prefix() {
    local install_prefix="$1"

    # Check if the installation directory already exists
    if [[ -e "$install_prefix" ]]; then
        backup_previous_installation "$install_prefix"
    fi

    # Run retention exactly once per invocation.
    # This still cleans pre-existing backup clutter when no current install exists.
    prune_old_backups "$install_prefix" "$RETAIN_BACKUPS"

    # Create the installation directory
    mkdir -p "$install_prefix" || die "Failed to create installation directory: $install_prefix"

    # Prevent recursive copy if install_prefix is inside tarball_path
    if [[ "$(readlink -f "$install_prefix")" == "$(readlink -f "$tarball_path")"* ]]; then
        die "Cannot install into a subdirectory of the source package."
    fi

    echo " - Installing Firestorm Viewer to $install_prefix"

    # Copy all files from the tarball to the installation directory
    cp -a "${tarball_path}/." "$install_prefix/" || die "Failed to complete the installation!"
    echo " - Installation completed successfully."
}

# Function for user-specific installation (non-root)
homedir_install() {
    warn "You are not running as a privileged user, so you will only be able"
    warn "to install the Firestorm Viewer in your home directory. If you"
    warn "would like to install the Firestorm Viewer system-wide, please run"
    warn "this script as the root user, or with the 'sudo' command."
    echo

    local install_prefix
    if [[ -n "$CUSTOM_INSTALL_DIR" ]]; then
        install_prefix="$CUSTOM_INSTALL_DIR"
    elif [[ -n "$SUFFIX" ]]; then
        install_prefix="${DEFAULT_USER_INSTALL_DIR}_${SUFFIX}"
    else
        install_prefix="$DEFAULT_USER_INSTALL_DIR"
    fi

    if [[ "$NON_INTERACTIVE" != "true" ]]; then
        echo -n "Enter the desired installation directory [${install_prefix}]: "
        read -r user_input
        if [[ -n "$user_input" ]]; then
            install_prefix="$user_input"
        fi
    fi

    install_to_prefix "${install_prefix}"
    "${install_prefix}/etc/refresh_desktop_app_entry.sh"
}

# Function for system-wide installation (root)
root_install() {
    local default_prefix="$DEFAULT_SYSTEM_INSTALL_DIR"

    # Determine the installation directory based on suffix or custom directory
    if [[ -n "$CUSTOM_INSTALL_DIR" ]]; then
        local install_prefix="$CUSTOM_INSTALL_DIR"
    elif [[ -n "$SUFFIX" ]]; then
        local install_prefix="$default_prefix_$SUFFIX"
    else
        local install_prefix="$default_prefix"
    fi

    if [[ "$NON_INTERACTIVE" != "true" ]]; then
        echo -n "Enter the desired installation directory [${install_prefix}]: "
        read -r user_input
        if [[ -n "$user_input" ]]; then
            install_prefix="$user_input"
        fi
    fi

    install_to_prefix "$install_prefix"

    # Ensure the applications directory exists
    mkdir -p /usr/local/share/applications || die "Failed to create /usr/local/share/applications"

    "${install_prefix}/etc/refresh_desktop_app_entry.sh"
}

# Function to parse command-line arguments
parse_arguments() {
    while [[ "$#" -gt 0 ]]; do
        case $1 in
            --suffix)
                if [[ -n "$2" && ! "$2" =~ ^-- ]]; then
                    SUFFIX="$2"
                    shift 2
                else
                    die "Error: --suffix requires a non-empty option argument."
                fi
                ;;
            --install-dir)
                if [[ -n "$2" && ! "$2" =~ ^-- ]]; then
                    CUSTOM_INSTALL_DIR="$2"
                    shift 2
                else
                    die "Error: --install-dir requires a non-empty option argument."
                fi
                ;;
            --retain)
                if [[ -n "$2" && "$2" =~ ^[0-9]+$ ]]; then
                    RETAIN_BACKUPS="$2"
                    shift 2
                else
                    die "Error: --retain requires a non-negative integer argument."
                fi
                ;;
            --yes|-y)
                NON_INTERACTIVE="true"
                shift
                ;;
            --help|-h)
                usage
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
    done
}

# Main installation workflow
main() {
    echo "Starting Firestorm Viewer installation script..."

    RETAIN_BACKUPS="${DEFAULT_RETAIN_BACKUPS}"
    NON_INTERACTIVE="false"

    # Parse command-line arguments
    parse_arguments "$@"

    echo "Installation Options:"
    echo "  Suffix: ${SUFFIX:-None}"
    echo "  Custom Install Directory: ${CUSTOM_INSTALL_DIR:-None}"
    echo "  Retained Backups: ${RETAIN_BACKUPS}"
    echo "  Non-interactive: ${NON_INTERACTIVE}"
    echo

    # Determine if the script is run as root
    if [[ "$EUID" -eq 0 ]]; then
        root_install
    else
        homedir_install
    fi

    echo "Firestorm Viewer installation process completed successfully."
}

# Execute the main function with all passed arguments
main "$@"
