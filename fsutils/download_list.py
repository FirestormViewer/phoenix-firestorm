#!/bin/python3
import argparse
import os
import sys
import time
import zipfile
import glob
import shutil
import hashlib
import pytz
from datetime import datetime
import requests
from discord_webhook import DiscordWebhook
from pathlib import Path
from typing import Tuple

from build_config import BuildConfig

def get_current_date_str():
    now = datetime.now(pytz.timezone('UTC'))
    day = now.day
    month = now.month
    year = now.year
    return f"{day}{month}{year}"

def generate_secret(secret_key):
    current_date = get_current_date_str()
    data = secret_key + current_date
    secret_for_api = hashlib.sha1(data.encode()).hexdigest()
    return secret_for_api

# run a command line subshell and return the output

# We want to get the following output by looping over the files
# DOWNLOADS
# -------------------------------------------------------------------------------------------------------

# Windows for SL 64 bit
# https://downloads.firestormviewer.org/preview/windows/Phoenix-Firestorm-Releasex64-6-6-8-68355_Setup.exe

# MD5: 3094776F5DB11B6A959B0F3AED068C6A

# Windows for SL 32 bit
# https://downloads.firestormviewer.org/preview/windows/Phoenix-Firestorm-Release-6-6-8-68355_Setup.exe

# MD5: 2F960B3353971FFF63307B5210D306F5

# Windows for Opensim 64bit
# https://downloads.firestormviewer.org/preview/windows/Phoenix-FirestormOS-Releasex64-6-6-8-68355_Setup.exe

# MD5: 6218D7B826538BB956699F9581532ECE

# Windows for Opensim 32bit
# https://downloads.firestormviewer.org/preview/windows/Phoenix-FirestormOS-Release-6-6-8-68355_Setup.exe

# MD5: D636CAFD287B4C8B96D726FE2A145327

# -------------------------------------------------------------------------------------------------------
# Mac OSX for SL
# https://downloads.firestormviewer.org/preview/mac/Phoenix-Firestorm-Releasex64-6-6-8-68355.dmg

# MD5: DA5AF534690328078B0B7BCEEA8D6959

# Mac OSX for Opensim
# https://downloads.firestormviewer.org/preview/mac/Phoenix-FirestormOS-Releasex64-6-6-8-68355.dmg

# MD5: 16CA020E73760D8205E2314D07EEC90E

# -------------------------------------------------------------------------------------------------------
# Linux for SL
# https://downloads.firestormviewer.org/preview/linux/Phoenix-Firestorm-Releasex64-6-6-8-68355.tar.xz

# MD5: 1A0C50065077B92889FFBC651E4278E4

# Linux for Opensim
# https://downloads.firestormviewer.org/preview/linux/Phoenix-FirestormOS-Releasex64-6-6-8-68355.tar.xz

# MD5: 9D5D8021F376194B42F6E7D8E537E45E

# -------------------------------------------------------------------------------------------------------
# iterate over the files in a directory and pass them to a command line subshell
def get_files(path):
    files = []
    for root, dirs, filenames in os.walk(path):        
        for filename in filenames:
            files.append(filename)
    print(f"Found : {files} on {path}")
    return files    

def run_cmd(cmd):
    # print(cmd)
    return os.popen(cmd).read()

#using the md5sum command get the md5 for the file

def get_md5(mdfile):
    md5sum = run_cmd(f"md5sum {mdfile}")
    #split md5sum on space
    md5sum = md5sum.split()[0]
    #remove leading '\'
    if md5sum[0] == "\\":
        md5sum = md5sum[1:]
    print(f"generating md5sum for {mdfile} as {md5sum}")
    return md5sum

def unzip_file(zip_file, unzip_dir):
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
        zip_ref.extractall(unzip_dir)

def flatten_tree(tree_root):
    print(f"Flattening tree {tree_root}")
    for root, flatten_dirs, files in os.walk(tree_root, topdown=False):
        for file in files:
            # Construct the full path to the file
            file_path = os.path.join(root, file)
            # Move the file to the root directory
            shutil.move(file_path, tree_root)
        for dir in flatten_dirs:
            # Construct the full path to the subdirectory
            subdir_path = os.path.join(root, dir)
            # Delete the subdirectory and its contents
            shutil.rmtree(subdir_path)

def get_build_variables():
    """
    Extracts initial build variables from environment variables.
    In practice these are set from the outputs of the earlier matrix commands.
    Returns:
        dict: A dictionary containing 'version' and 'build_number'.
    """
    import os

    version = os.environ.get('FS_VIEWER_VERSION')
    build_number = os.environ.get('FS_VIEWER_BUILD')
    release_type = os.environ.get('FS_VIEWER_RELEASE_TYPE')

    if not version or not build_number or not release_type:
        raise ValueError("Environment variables 'FS_VIEWER_VERSION' and 'FS_VIEWER_BUILD' must be set.")

    return {
        'version': version,
        'build_number': build_number,
        'version_full': f"{version}.{build_number}",
        'release_type': release_type,
    }

def get_hosted_folder_for_build_type(build_type, config):
    return config.build_type_hosted_folder.get(
        build_type, 
        config.build_type_hosted_folder.get("Unknown")
        )

def is_supported_build_type(build_type, config):
    if build_type in config.build_type_hosted_folder:
        return True
    else:
        return False
def get_hosted_folder_for_os_type(os_type, config):
    return config.os_hosted_folder.get(
        os_type
        )

def get_supported_os(os_name, config):
    # throws for unexpected os_name
    return config.os_hosted_folder.get(os_name)

def extract_vars_from_zipfile_name(file_path: str) -> Tuple[str, str, str, str, str, str]:
    """
    Extract variables from an artifact zip file name.

    The expected filename format is:
        {build_Type}-{platform}-{platform_ver}-{variant}-{grid}-artifacts.zip

    Args:
        file_path (str): The full path to the zip file.

    Returns:
        Tuple containing:
            filename (str): The basename of the file.
            build_type (str)
            platform (str)
            platform_ver (str)
            variant (str)
            grid (str)

    Raises:
        ValueError: If the filename does not conform to the expected format.
    """
    # Use pathlib for path manipulations
    path = Path(file_path)
    filename = path.name

    # Remove the .zip extension
    name_without_ext = path.stem

    # Expected suffix after the main parts
    expected_suffix = "artifacts"
    parts = name_without_ext.split('-')

    if len(parts) < 6:
        raise ValueError(f"Filename '{filename}' does not have enough parts separated by '-'.")

    # Unpack the parts
    build_type, platform, platform_ver, variant, grid, suffix = parts[:6]

    if suffix.lower() != expected_suffix:
        raise ValueError(f"Filename '{filename}' does not end with '-{expected_suffix}'.")

    # Return the extracted variables, applying lower() where needed
    return (
        filename,
        build_type,
        platform.lower(),
        platform_ver.lower(),
        variant.lower(),
        grid.lower()
    )

def unpack_artifacts(path_to_artifacts_directory, config):
    build_types_found = {}
    zips = glob.glob(f"{path_to_artifacts_directory}/*.zip")
    for file in zips:
        print(f"Processing zip file {file}")
        try:
            filename, build_type, platform, platform_ver, variant, grid = extract_vars_from_zipfile_name(file)
            # print(f"Filename: {filename}")
            # print(f"Build Type: {build_type}")
            # print(f"Platform: {platform}")
            # print(f"Platform Version: {platform_ver}")
            # print(f"Variant: {variant}")
            # print(f"Grid: {grid}")
        except ValueError as e:
            print(f"Error extracting vars from zipfile name: {e}")
            continue
        print(f"Identified filename {filename} with  build_type {build_type} and platform {platform}({platform_ver}) grid {grid} and variant {variant} from file {file}")
        if is_supported_build_type( build_type, config) == False:
            print(f"Invalid build_type {build_type} from file {file} using 'Unknown' instead")
            build_type = "Unknown"
        else:
            print(f"Using build_type {build_type} from file {file}")

        build_folder = get_hosted_folder_for_build_type(build_type, config)
        print(f"build_folder {build_folder}")
        try:
            build_type_dir = os.path.join(path_to_artifacts_directory, build_folder)
        except Exception as e:
            print(f"An error occurred while creating build_type_dir folder from {path_to_artifacts_directory} and {build_folder}: {e}")
            continue
        print(f"build_type_dir {build_type_dir}")
        os_folder = get_hosted_folder_for_os_type(platform, config)
        print(f"os_folder {os_folder}")
        try:
            unpack_folder = os.path.join(build_type_dir, os_folder)
        except Exception as e:
            print(f"An error occurred while creating unpack_folder folder from {build_type_dir} and {os_folder}: {e}")
            continue
        print(f"unpacking {filename} to {unpack_folder}")
        if os.path.isfile(file):
            # this is an actual zip file
            try:
                unzip_file(file, unpack_folder)
            except zipfile.BadZipFile:
                print(f"Skipping {file} as it is not a valid zip file")
                continue
            except Exception as e:
                print(f"An error occurred while unpacking {file}: {e} , skipping file {filename}")
                continue
        else:
            # Create the destination folder if it doesn't exist
            # if not os.path.exists(unpack_folder):
            #     os.makedirs(unpack_folder)
            # Copy the contents of the source folder to the destination folder recursively
            shutil.copytree(file, unpack_folder, dirs_exist_ok=True)
        print(f"Finished unpacking {filename} to {unpack_folder}")
        if build_type not in build_types_found:
            print(f"Creating build_type {build_type} entry in build_types_found")
            build_types_found[build_type] = { 
                "build_type": build_type,
                "build_type_folder": build_folder,
                "build_type_fullpath": build_type_dir,
                "os_folders": [], 
            }
        if os_folder not in build_types_found[build_type]["os_folders"]:
            build_types_found[build_type]["os_folders"].append(os_folder)
            print(f"Appended {os_folder} to build_type {build_type}")
    print(f"Finished processing artifacts for build_type {build_type}")
    return build_types_found

def restructure_folders(build_type_info, config):
    print(f"Restructuring folders for build_type {build_type_info['build_type']}")
    build_type_dir = build_type_info["build_type_fullpath"]
    if not os.path.exists(build_type_dir):
        print(f"Unexpected error: path {build_type_dir} does not exist, even though it was in the set.")
        raise FileNotFoundError
    # loop over the folder in the build_type_dir
    for platform_folder in build_type_info["os_folders"]:
        print(f"Cleaning up {platform_folder}")
        # Traverse the directory tree and move all of the files to the root directory
        flatten_tree(os.path.join(build_type_dir, platform_folder))
    # Now move the symbols files to the symbols folder
    # Define the folder for symbols
    symbols_folder = os.path.join(build_type_dir, "symbols")
    os.mkdir(symbols_folder)
    # prep the symbols folder
    symbol_patterns = ["*_hvk*", "*_oss*"]

    # Loop through each pattern, find matching files, and move them
    for pattern in symbol_patterns:
        symbol_archives = glob.glob(f"{build_type_dir}/**/{pattern}", recursive=True)
        for sym_file in symbol_archives:
            print(f"Moving {sym_file} to {symbols_folder}")
            shutil.move(sym_file, symbols_folder)

def gather_build_info(build_type_info, config):
    print(f"Gathering build info for build_type {build_type_info}")
    # While we're at it, let's print the md5 listing 
    download_root = f"{config.download_root}/{build_type_info['build_type_folder']}"
    # for each os that we have built for 
    build_type_dir = build_type_info["build_type_fullpath"]
    for platform_folder in build_type_info["os_folders"]:
        print(f"Getting files for {platform_folder} in {build_type_dir}")
        build_type_platform_folder = os.path.join(build_type_dir, platform_folder)
        files = get_files(build_type_platform_folder)
        try:
            for file in files:
                full_file = os.path.join(build_type_platform_folder, file)
                base_name = os.path.basename(file)
                file_URI = f"{download_root}/{platform_folder}/{base_name}"
                md5 = get_md5(full_file)
                
                if "AyaneStormOS-" in base_name:
                    grid = "OS"
                else:
                    grid = "SL"

                if "x64" in base_name:
                    variant = "avx"
                else:
                    variant = "regular"

                file_key = f"{grid}-{variant}-{platform_folder}"

                # if platform_folder in config.os_download_dirs:
                if "downloadable_artifacts" not in build_type_info:
                    build_type_info["downloadable_artifacts"] = {}

                build_type_info["downloadable_artifacts"][f"{file_key}"] = {
                    "file_path": full_file,         
                    "file_download_URI": file_URI,
                    "grid": grid,
                    "variant": variant,
                    "fs_ver_mgr_platform": config.fs_version_mgr_platform.get(platform_folder),
                    "md5": md5,
                }

        except TypeError:
            print(f"Error processing files for {platform_folder} in {build_type_dir}")
            continue
        except Exception as e:
            print(f"An error occurred while processing files for {platform_folder} in {build_type_dir}: {e}")
            continue
        print(f"Created build info: {build_type_info}")
    return build_type_info

def create_discord_message(build_info, config):
# Start with a header line            
    text_summary = f'''
DOWNLOADS - {build_info["build_type"]}
-------------------------------------------------------------------------------------------------------
'''
# for each platform we potentailly build for 
# Append platform label in printable form
    for platform_folder in config.supported_os_dirs:
        platform_printable = config.platforms_printable[platform_folder]
        text_summary += f'''
{platform_printable}
'''
        platform_folder = platform_folder.lower()
        for grid in ["SL", "OS"]:
            grid_printable = f"{config.grids_printable[grid]}"
            for variant in ["avx", "regular"]:
                variant_printable = f"{config.variant_printable[variant]}"
                text_summary += f"{variant_printable}\n"
                try:
                    file_key = f"{grid}-{variant}-{platform_folder}"
                    text_summary += f"{platform_printable} for {grid_printable}\n"
                    text_summary += f"{build_info['downloadable_artifacts'][file_key]['file_download_URI']}\n"
                    text_summary += "\n"
                    text_summary += f"MD5: {build_info['downloadable_artifacts'][file_key]['md5']}\n"
                    text_summary += "\n"
                except KeyError:
                    text_summary += f"{platform_printable} for {grid_printable} - NOT AVAILABLE\n"
                    text_summary += "\n"
        text_summary += '''
-------------------------------------------------------------------------------------------------------
'''
    return text_summary

def update_fs_version_mgr(build_info, config):
    print(f"Updating Firestorm Version Manager for build_type {build_info['build_type']}")
    # Read the secret key from environment variables
    secret_key = os.environ.get('FS_VERSION_MGR_KEY')
    if not secret_key:
        print("Error: FS_VERSION_MGR_KEY not set")
        sys.exit(1)

    secret_for_api = generate_secret(secret_key)  
    build_type = build_info["build_type"].lower()
    version = os.environ.get('FS_VIEWER_VERSION')
    build_number = os.environ.get('FS_VIEWER_BUILD')

    for file_key in build_info["downloadable_artifacts"]:
        try:
            download_link = build_info["downloadable_artifacts"][file_key]["file_download_URI"]
            md5_checksum = build_info["downloadable_artifacts"][file_key]["md5"]
            grid = build_info["downloadable_artifacts"][file_key]["grid"].lower()
            build_variant = build_info["downloadable_artifacts"][file_key]["variant"].lower()
            os_name = build_info["downloadable_artifacts"][file_key]["fs_ver_mgr_platform"]
        except KeyError:
            print(f"Error: Could not find downloadable artifacts for {file_key}")
            continue

        payload = {
            "viewer_channel": build_type,
            "grid_type": grid,
            "operating_system": os_name,
            "build_type": build_variant, # on the webservice variant is known as build_type.
            "viewer_version": version,
            "build_number": int(build_number),
            "download_link": download_link,
            "md5_checksum": md5_checksum
        }
        print(f"Payload (without secret): {payload}")
        payload["secret"] = secret_for_api

        # Make the API call
        url = "https://www.firestormviewer.org/set-fs-vrsns-jsn/"
        headers = {"Content-Type": "application/json"}

        response = None  # Initialize response to None

        try:
            response = requests.post(url, json=payload, headers=headers)
            
            # Manually check for status code instead of raising an exception
            if response.status_code == 200:
                response_data = response.json()
                result = response_data.get('result')
                message = response_data.get('message')

                if result == 'success':
                    print(f"Version manager updated successfully for {os_name} {build_variant}")
                else:
                    print(f"Error updating version manager: {message}")
            else:
                print(f"Unexpected status code received: {response.status_code}")
                print(f"Response body: {response.text}")

        except requests.exceptions.RequestException as e:
            print(f"API request failed: {e}")
            
            # Additional error handling
            if response and response.status_code == 403:
                print("Status Code:", response.status_code)
                print("Response Headers:", response.headers)
                print("Response Body:", response.text)

        except ValueError:
            print("API response is not valid JSON")

def split_discord_text_on_separator(discord_text, max_length=2000, separator=None):
    if separator is None:
        separator = "-" * 103  

    # Split the text on the separator lines
    chunks = discord_text.split(separator + '\n')

    messages = []

    for index, chunk in enumerate(chunks):
        # Strip leading/trailing whitespace
        chunk = chunk.strip('\n')

        # Re-add the separator to the end of each chunk except the last one
        if index < len(chunks) - 1:
            chunk += '\n' + separator

        # Split the chunk further if it exceeds max_length
        while len(chunk) > max_length:
            # Find the last newline before max_length
            split_point = chunk.rfind('\n', 0, max_length)
            if split_point == -1 or split_point == 0:
                # Can't find a newline, split at max_length
                split_point = max_length

            part = chunk[:split_point].rstrip('\n')
            messages.append(part)
            chunk = chunk[split_point:].lstrip('\n')

        if chunk:
            messages.append(chunk)

    return messages

# parse args first arg optional -r (release) second arg mandatory string path_to_directory
def main():
    try:
        # Initialise the build configuration
        config = BuildConfig()

        parser = argparse.ArgumentParser(
            prog="print_download_list",
            description="Prints the list of files for download and their md5 checksums"
            )
        parser.add_argument("-w", "--webhook", help="post details to the webhook")

        # add path_to_directory required parameter to parser
        parser.add_argument("path_to_directory", help="path to the directory in which we'll look for the files")

        args = parser.parse_args()

        # Create a webhook object with the webhook URL
        if args.webhook:
            webhook = DiscordWebhook(url=args.webhook)

        # unzip the github artifact for this OS (`dir`) into the folder `dir`
        # get the .zip files in args.path_to_directory using glob 
        print(f"Processing artifacts in {args.path_to_directory}")
        build_types_created = unpack_artifacts(args.path_to_directory, config)
        print(f"buuild types created: {build_types_created}")
        for build_type_key, build_type_info in build_types_created.items():            
            print(f"Processing {build_type_key}")
            restructure_folders(build_type_info, config)
            build_info = gather_build_info(build_type_info, config)
            update_fs_version_mgr(build_info, config)

            discord_text = create_discord_message(build_info, config)
            if args.webhook:
                messages = split_discord_text_on_separator(discord_text)
                for message in messages:
                    webhook.set_content(content=message)
                    response = webhook.execute()
                    if not response.ok:
                        print(f"Webhook Error {response.status_code}: {response.text}")
            print(discord_text)
    except Exception as e:
        print(f"An error occurred: {e}")
        sys.exit(1)        

if __name__ == '__main__':
    import sys
    main()