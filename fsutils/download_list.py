#!/bin/python3
import argparse
import os
import sys
import time
import zipfile
import glob
import shutil

# iterate over the files in a directory and pass them to a command line subshell
def get_files(path):
    files = []
    for root, dirs, files in os.walk(path):        
        # print(f"Found : {files}")
        return files    
    return None


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

def run_cmd(cmd):
    # print(cmd)
    return os.popen(cmd).read()

#using the md5sum command get the md5 for the file

def get_md5(mdfile):
    # print(f"mdfile is {mdfile}")
    md5sum = run_cmd(f"md5sum {mdfile}")
    #split md5sum on space
    md5sum = md5sum.split()[0]
    #remove leading '\'
    md5sum = md5sum[1:]
    return md5sum

def unzip_file(zip_file, unzip_dir):
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
        zip_ref.extractall(unzip_dir)

def flatten_tree(tree_root):
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
# add path_to_directory required parameter to parser
parser.add_argument("path_to_directory", help="path to the directory in which we'll look for the files")

args = parser.parse_args()
path_to_directory = args.path_to_directory
release = args.release

dirs = ["windows", "mac", "linux"]

if args.unzip:
    # unzip the github artifact for this OS (`dir`) into the folder `dir`
    # get the .zip files in args.path_to_directory using glob 
    zips = glob.glob(f"{args.path_to_directory}/*.zip")
    for file in zips:
        # print(f"unzipping {file}")
        if "ubuntu" in file.lower():
            unzip_file(file, os.path.join(args.path_to_directory, "linux"))
        if "windows" in file.lower():
            unzip_file(file, os.path.join(args.path_to_directory, "windows"))
        if "macos" in file.lower():
            unzip_file(file, os.path.join(args.path_to_directory, "mac"))
    for dir in dirs:
        flatten_tree(os.path.join(args.path_to_directory, dir))
    # Now move the symbols files to the symbols folder
    symbols_folder = os.path.join(args.path_to_directory, "symbols")
    os.mkdir(symbols_folder)
    # Traverse the directory tree and move all of the files to the root directory
    symbol_archives = glob.glob(f"{args.path_to_directory}/**/*_hvk*", recursive=True)
    for sym_file in symbol_archives:
        print(f"Moving {sym_file} to {symbols_folder}")
        shutil.move(sym_file, symbols_folder)
    symbol_archives = glob.glob(f"{args.path_to_directory}/**/*_oss*", recursive=True)
    for sym_file in symbol_archives:
        print(f"Moving {sym_file} to {symbols_folder}")
        shutil.move(sym_file, symbols_folder)

        
file_dict = {}
md5_dict = {}

for dir in dirs:
    dir = dir.lower()
    files = get_files(os.path.join(args.path_to_directory, dir))
    for file in files:
        full_file = os.path.join(args.path_to_directory, dir, file)
        md5 = get_md5(full_file)
        base_name = os.path.basename(file)
        if "-Release-" in base_name or "-Beta-" in base_name:
            wordsize = "32"
        else:
            wordsize = "64"
        
        if "FirestormOS-" in base_name:
            grid = "OS"
        else:
            grid = "SL"

        if dir in dirs:
            file_dict[f"{grid}{dir}{wordsize}"] = full_file
            md5_dict[f"{grid}{dir}{wordsize}"] = md5

download_root_preview = "https://downloads.firestormviewer.org/preview"        
download_root_release = "https://downloads.firestormviewer.org/release"        

if args.release:
    download_root = download_root_release
else:
    download_root = download_root_preview

print('''
DOWNLOADS''')

platforms_printable = {"windows":"MS Windows", "mac":"MacOS", "linux":"Linux"}
grids_printable = {"SL":"Second Life", "OS":"OpenSim"}

for dir in dirs:
    print(f'''-------------------------------------------------------------------------------------------------------
{platforms_printable[dir]}
''')
    dir=dir.lower()
    wordsize = "64"
    platform = f"{platforms_printable[dir]}"
    for grid in ["SL", "OS"]:
        grid_printable = f"{grids_printable[grid]}"
        try:
            print (f"{platform} for {grid_printable} ({wordsize}-bit)")
            print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict[f"{grid}{dir}{wordsize}"])) )
            print ()
            print ( "MD5: {}".format(md5_dict[f"{grid}{dir}{wordsize}"]) )
            print ()
            if(dir == "windows"):
                # Need to do 32 bit as well
                wordsize = "32"
                print (f"{platform} for {grid_printable} ({wordsize}-bit)")
                print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict[f"{grid}{dir}{wordsize}"])) )
                print ()
                print ( "MD5: {}".format(md5_dict[f"{grid}{dir}{wordsize}"]) )
                print ()
                wordsize = "64"
        except KeyError:
            print (f"{platform} for {grid_printable} ({wordsize}-bit) - NOT AVAILABLE")
            print ()

print('''
-------------------------------------------------------------------------------------------------------''')

