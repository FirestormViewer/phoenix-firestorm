#!/bin/python3
import argparse
import os
import sys
import time
import zipfile
import glob
import shutil
from discord_webhook import DiscordWebhook




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


# parse args first arg optional -r (release) second arg mandatory string path_to_directory

parser = argparse.ArgumentParser(
    prog="print_download_list",
    description="Prints the list of files for download and their md5 checksums"
    )
parser.add_argument("-r", "--release", required=False, default=False, action="store_true", help="use the release folder in the target URL")
parser.add_argument("-u", "--unzip", required=False, default=False, action="store_true", help="unzip the github artifact first")
parser.add_argument("-w", "--webhook", help="post details to the webhook")

# add path_to_directory required parameter to parser
parser.add_argument("path_to_directory", help="path to the directory in which we'll look for the files")

args = parser.parse_args()
path_to_directory = args.path_to_directory
release = args.release

# Create a webhook object with the webhook URL
if args.webhook:
    webhook = DiscordWebhook(url=args.webhook)

dirs = ["windows", "mac", "linux"]

# build_types is a map from Beta, Release and Nightly to folder names preview release and nightly
build_types = {
    "Alpha": "test",
    "Beta": "preview",
    "Release": "release",
    "Nightly": "nightly",
    "Unknown": "test"
}

target_folder = {
    "ubuntu":"linux",
    "windows":"windows",
    "macos":"mac"
}

# unzip the github artifact for this OS (`dir`) into the folder `dir`
# get the .zip files in args.path_to_directory using glob 
print(f"Processing artifacts in {args.path_to_directory}")
build_types_created = set()
zips = glob.glob(f"{args.path_to_directory}/*.zip")
for file in zips:
    # print(f"unzipping {file}")
    #extract first word (delimited by '-' from the file name)
    # build_type is a fullpath but we only want the last folder, remove the leading part of the path leaving just the foldername using basename
    filename = os.path.basename(file)
    build_type = filename.split("-")[0]
    platform = filename.split("-")[1].lower()

    # print(f"build_type is {build_type}")
    if build_type not in build_types:
        print(f"Invalid build_type {build_type} from file {file} using 'Unknown'")
        build_type = "Unknown"

    build_folder = build_types[build_type]
    
    build_types_created.add(build_type)

    build_type_dir = os.path.join(args.path_to_directory, build_folder)

    if platform not in target_folder:
        print(f"Invalid platform {platform} using file {file}")
        continue
    
    unpack_folder = os.path.join(build_type_dir, target_folder[platform])
    print(f"unpacking {filename} to {unpack_folder}")

    if os.path.isfile(file):
        # this is an actual zip file
        unzip_file(file, unpack_folder)
    else:
        # Create the destination folder if it doesn't exist
        # if not os.path.exists(unpack_folder):
        #     os.makedirs(unpack_folder)
        # Copy the contents of the source folder to the destination folder recursively
        shutil.copytree(file, unpack_folder, dirs_exist_ok=True)

output = ""
for build_type in build_types_created:
    build_type_dir = os.path.join(args.path_to_directory, build_types[build_type])
    if not os.path.exists(build_type_dir):
        print(f"Unexpected error: {build_type_dir} does not exist, even though it was in the set.")
        continue
    # loop over the folder in the build_type_dir
    for dir in dirs:
        print(f"Cleaning up {dir}")
        # Traverse the directory tree and move all of the files to the root directory
        flatten_tree(os.path.join(build_type_dir, dir))
    # Now move the symbols files to the symbols folder
    # prep the symbols folder
    symbols_folder = os.path.join(build_type_dir, "symbols")
    os.mkdir(symbols_folder)
    symbol_archives = glob.glob(f"{build_type_dir}/**/*_hvk*", recursive=True)
    for sym_file in symbol_archives:
        print(f"Moving {sym_file} to {symbols_folder}")
        shutil.move(sym_file, symbols_folder)
    symbol_archives = glob.glob(f"{build_type_dir}/**/*_oss*", recursive=True)
    for sym_file in symbol_archives:
        print(f"Moving {sym_file} to {symbols_folder}")
        shutil.move(sym_file, symbols_folder)

    # While we're at it, let's print the md5 listing 
    file_dict = {}
    md5_dict = {}
    platforms_printable = {"windows":"MS Windows", "mac":"MacOS", "linux":"Linux"}
    grids_printable = {"SL":"Second Life", "OS":"OpenSim"}

    download_root = f"https://downloads.firestormviewer.org/{build_types[build_type]}"
    output += f'''
DOWNLOADS - {build_type}
-------------------------------------------------------------------------------------------------------
'''
    for dir in dirs:
        print(f"Getting files for {dir} in {build_type_dir}")
        files = get_files(os.path.join(build_type_dir, dir))
        try:
            for file in files:
                full_file = os.path.join(build_type_dir, dir, file)
                md5 = get_md5(full_file)
                base_name = os.path.basename(file)
                if "x64" in base_name:
                    wordsize = "64"
                else:
                    wordsize = "32"
                
                if "FirestormOS-" in base_name:
                    grid = "OS"
                else:
                    grid = "SL"

                if dir in dirs:
                    file_dict[f"{grid}{dir}{wordsize}"] = full_file
                    md5_dict[f"{grid}{dir}{wordsize}"] = md5
        except TypeError:
            print(f"No files found for {dir} in {build_type_dir}")



        output += f'''
{platforms_printable[dir]}
'''
        dir = dir.lower()
        wordsize = "64"
        platform = f"{platforms_printable[dir]}"
        for grid in ["SL", "OS"]:
            grid_printable = f"{grids_printable[grid]}"
            try:
                output += f"{platform} for {grid_printable} ({wordsize}-bit)\n"
                output += f"{download_root}/{dir}/{os.path.basename(file_dict[f'{grid}{dir}{wordsize}'])}\n"
                output += "\n"
                output += f"MD5: {md5_dict[f'{grid}{dir}{wordsize}']}\n"
                output += "\n"
                if dir == "windows":
                    # Need to do 32 bit as well
                    wordsize = "32"
                    output += f"{platform} for {grid_printable} ({wordsize}-bit)\n"
                    output += f"{download_root}/{dir}/{os.path.basename(file_dict[f'{grid}{dir}{wordsize}'])}\n"
                    output += "\n"
                    output += f"MD5: {md5_dict[f'{grid}{dir}{wordsize}']}\n"
                    output += "\n"
                    wordsize = "64"
            except KeyError:
                output += f"{platform} for {grid_printable} ({wordsize}-bit) - NOT AVAILABLE\n"
                output += "\n"
        output += '''-------------------------------------------------------------------------------------------------------
'''

    if args.webhook:
        # Add the message to the webhook
        webhook.set_content(content=output)
        # Send the webhook
        response = webhook.execute()
        # Print the response
        if not response.ok:
            print(f"Webhook Error {response.status_code}: {response.text}")        
    print(output)

