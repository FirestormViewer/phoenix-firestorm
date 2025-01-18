#!/bin/bash

# Enhanced Firestorm Viewer Installation Script
# Features:
#   - Creates a timestamped backup of existing installation
#   - Allows installation with custom suffixes for parallel versions (e.g., beta, release, debug)
#   - Supports both system-wide and user-specific installations
#   - Provides command-line options for flexibility

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

# Backup directory base
BACKUP_BASE_DIR="${DEFAULT_SYSTEM_INSTALL_DIR}_backups"

# Function to display usage information
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --suffix <suffix>         Append a suffix to the installation directory (e.g., beta, release, debug)"
    echo "  --install-dir <directory> Specify a custom installation directory"
    echo "  --help, -h                Display this help message"
    exit 1
}

# Function to prompt the user with a yes/no question
prompt() {
    local prompt_message="$1"
    local input

    echo -n "$prompt_message"

    while read -r input; do
        case "$input" in
            [Yy]* )
                return 1
                ;;
            [Nn]* )
                return 0
                ;;
            * )
                echo "Please enter yes or no."
                echo -n "$prompt_message"
                ;;
        esac
    done
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

# Function to install Firestorm Viewer to the specified directory
install_to_prefix() {
    local install_prefix="$1"

    # Check if the installation directory already exists
    if [[ -e "$install_prefix" ]]; then
        backup_previous_installation "$install_prefix"
    fi

    # Create the installation directory
    mkdir -p "$install_prefix" || die "Failed to create installation directory: $install_prefix"

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

    local install_prefix="$DEFAULT_USER_INSTALL_DIR"
    if [[ -n "$SUFFIX" ]]; then
        install_prefix="${install_prefix}_${SUFFIX}"
    fi

    echo -n "Enter the desired installation directory [${install_prefix}]: "
    read -r user_input
    if [[ -n "$user_input" ]]; then
        install_prefix="$user_input"
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

    echo -n "Enter the desired installation directory [${install_prefix}]: "
    read -r user_input
    if [[ -n "$user_input" ]]; then
        install_prefix="$user_input"
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
            --help|-h)
                usage
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
    done
}

# Function to download Firestorm Viewer (optional enhancement)
download_firestorm() {
    # Define Firestorm Viewer download URL
    # NOTE: Replace this URL with the actual download link as needed
    FIRESTORM_URL="https://firestormviewer.org/download/linux/firestorm.tar.gz"

    # Temporary download location
    TEMP_DOWNLOAD="/tmp/firestorm.tar.gz"

    echo "Downloading Firestorm Viewer from $FIRESTORM_URL..."
    wget -O "$TEMP_DOWNLOAD" "$FIRESTORM_URL" || die "Failed to download Firestorm Viewer!"
    echo "Download completed."

    # Extract the tarball to the run path
    tar -xzf "$TEMP_DOWNLOAD" -C "$tarball_path" --strip-components=1 || die "Failed to extract Firestorm Viewer tarball!"
    echo "Extraction completed."

    # Clean up the downloaded tarball
    rm "$TEMP_DOWNLOAD"
}

# Main installation workflow
main() {
    echo "Starting Firestorm Viewer installation script..."

    # Parse command-line arguments
    parse_arguments "$@"

    echo "Installation Options:"
    echo "  Suffix: ${SUFFIX:-None}"
    echo "  Custom Install Directory: ${CUSTOM_INSTALL_DIR:-None}"
    echo

    # Optionally, download Firestorm Viewer if not already present
    # Uncomment the following line if you want the script to handle downloading
    # download_firestorm

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
