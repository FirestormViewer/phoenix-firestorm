#!/bin/python3
import argparse
import os
import sys
import time

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


# parse args first arg optional -r (release) second arg mandatory string path_to_directory

parser = argparse.ArgumentParser(
    prog="print_download_list",
    description="Prints the list of files for download and their md5 checksums"
    )
parser.add_argument("-r", "--release", required=False, default=False, action="store_true")
# add path_to_directory required parameter to parser
parser.add_argument("path_to_directory")

args = parser.parse_args()
path_to_directory = args.path_to_directory
release = args.release

platforms_printable = {"windows":"MS Windows", "mac":"MacOS", "linux":"Linux"}
dirs = ["windows", "mac", "linux"]

print('''
DOWNLOADS''')
file_dict = {}
md5_dict = {}
for dir in dirs:
    # print(f"looking in {os.path.join(sys.argv[1], dir)}")
    dir = dir.lower()
    files = get_files(os.path.join(args.path_to_directory, dir))
    for file in files:
        full_file = os.path.join(sys.argv[1], dir, file)
        md5 = get_md5(full_file)
        if(dir=="windows"):
            # print(f"testing {file} as {full_file}")
            if "Firestorm-Release-" in os.path.basename(file):
                file_dict["SLWin32"] = full_file
                md5_dict["SLWin32"] = md5
            elif "FirestormOS-Release-" in os.path.basename(file):
                file_dict["OSWin32"] = full_file
                md5_dict["OSWin32"] = md5
            elif "Firestorm-Releasex64-" in os.path.basename(file):
                file_dict["SLWin64"] = full_file
                md5_dict["SLWin64"] = md5
            elif "FirestormOS-Releasex64-" in os.path.basename(file):
                file_dict["OSWin64"] = full_file
                md5_dict["OSWin64"] = md5
        if(dir=="mac"):
            # print(f"testing {file} as {full_file}")
            if "Firestorm-Releasex64-" in os.path.basename(file):
                # print(f"storing {file} as SLMac64")
                file_dict["SLMac64"] = full_file
                md5_dict["SLMac64"] = md5
            elif "FirestormOS-Releasex64-" in os.path.basename(file):
                file_dict["OSMac64"] = full_file
                md5_dict["OSMac64"] = md5
        if(dir=="linux"):
            # print(f"testing {file} as {full_file}")
            if "Firestorm-Releasex64-" in os.path.basename(file):
                file_dict["SLLinux64"] = full_file
                md5_dict["SLLinux64"] = md5
            elif "FirestormOS-Releasex64-" in os.path.basename(file):
                file_dict["OSLinux64"] = full_file
                md5_dict["OSLinux64"] = md5

download_root_preview = "https://downloads.firestormviewer.org/preview"        
download_root_release = "https://downloads.firestormviewer.org/release"        

if args.release:
    download_root = download_root_release
else:
    download_root = download_root_preview

for dir in dirs:
    print(f'''-------------------------------------------------------------------------------------------------------
{platforms_printable[dir]}
''')
    dir=dir.lower()
    if(dir=="linux"):
        print ("Linux for SL")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["SLLinux64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["SLLinux64"]) )
        print ()
        print ("Linux for OpenSim")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["OSLinux64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["OSLinux64"]) )
        print ()
    if(dir=="mac"):
        print ("MacOS for SL")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["SLMac64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["SLMac64"]) )
        print ()
        print ("MacOS for OpenSim")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["OSMac64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["OSMac64"]) )
        print ()
    if(dir=="windows"):
        print ("Windows for SL 64-bit")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["SLWin64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["SLWin64"]) )
        print ()
        print ("Windows for SL 32-Bit")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["SLWin32"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["SLWin32"]) )
        print ()
        print ("Windows for OS 64-bit")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["OSWin64"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["OSWin64"]) )
        print ()
        print ("Windows for OS 32-Bit")
        print ( "{}/{}/{}".format(download_root,dir,os.path.basename(file_dict["OSWin32"])) )
        print ()
        print ( "MD5: {}".format(md5_dict["OSWin32"]) )
        print ()

print('''
-------------------------------------------------------------------------------------------------------''')



