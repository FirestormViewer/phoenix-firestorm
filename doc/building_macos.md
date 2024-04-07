# Build instructions for macOS

The viewer and its libraries are built with the current version of Xcode and Cmake, as well as the standard version of Autobuild from Linden Lab.

> [!WARNING]
> Please note that we do not give support for compiling the viewer on your own. However, there is a self-compilers group in Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

## Obtaining a shell to work with

The steps listed below are expected to be run from a shell prompt. Simply copy and paste the commands, or type them out if you wish, into a shell prompt when told to. If you do not know how to acquire a shell prompt, open up the Applications folder, go to Utilities, then open Terminal. This will open a shell prompt. If you are a power user and know what a shell prompt is, open one now using your preferred method and program, then continue on to the next section. Leave this window open throughout the guide. You don't need to close it and re-open it after every command.

## Getting development tools

You will need to install the following tools:

### Xcode
- Xcode, It's a free download from Apple, but the latest version (12.x) is too new, so you will need to get an older version.
- To download a version that will work (11.7 in this case), go [here](https://developer.apple.com/download) and log in with an apple ID.
- Either find the 11.7 download in the list or use this [direct link](https://download.developer.apple.com/Developer_Tools/Xcode_11.7/Xcode_11.7.xip) (will prompt you to log in if you skipped the above step.)
- Open the downloaded file and copy the Xcode application to the Applications folder.
- Open Xcode at least once to set up everything to compile the viewer later on.
- Test by running `clang --version` from the terminal window.
- It should report version 11 something (Apple clang version 11.0.3 (clang-1103.0.32.62) for example.)
- If it reports version 12, something is messed up and you installed Xcode 12.

### CMake
- Download [CMake](http://www.cmake.org/download) version 3.16.0 or higher
- Open the downloaded file and copy CMake to the Applications folder.
- You will need to install the command line links manually. To do this, run from the terminal: `sudo /Applications/CMake.app/Contents/MacOS/CMake --install`
- Again, test by running: `cmake --version` from the terminal window.

### Pip
The pip Python package installation tool is required for the next step. To install it, run from terminal:
```
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
sudo python3 get-pip.py
```

### Autobuild
The Linden Lab [Autobuild](https://github.com/secondlife/autobuild) tool.
- Use the following command to install it on your machine:
```
pip3 install --user -r requirements.txt
```

- Add it to your PATH environment variable so it can be found by the shell. The macOS-approved way to do this is to issue the following command (This change will not take effect until the next time you open a Terminal window.):
```
echo '~/Library/Python/3.7/bin/' | sudo tee /etc/paths.d/99-autobuild
```

- If you do not want to close and re-open your terminal, type the following to add it your PATH (otherwise if you did close it down, re-open it as shown above [here](#obtaining-a-shell-to-work-with)):
```
export PATH=$PATH:~/Library/Python/3.7/bin/
```

- Check Autobuild version to be "autobuild 3.9.3" or higher: `autobuild --version`

### Additional third party libraries 
If you want to use licensed FMOD Studio API or KDU build libraries (they are optional) you have to provide these yourself. If you're building Firestorm as part of the project team, ask for the libraries for fmodstudio and kdu. Put them into `/opt/firestorm`.

If you're a community builder, you'll need to build these libraries yourself, then change your autobuild.xml file to point to your own versions, or create a different autobuild.xml with your customizations, and use this with autobuild instead of our default autobuild.xml. There are some examples of how to build FMOD Studio on the LL Wiki and opensource-dev mailing list.

We've created a non-KDU build target to make this easier. Everywhere you see "ReleaseFS" below, use "ReleaseFS_open" instead. This will perform the same build, using openjpeg instead of KDU and omitting FMOD Studio.

Available premade firestorm-specific build targets:

```
ReleaseFS           (includes KDU, FMOD)
ReleaseFS_open      (no KDU, no FMOD)
RelWithDebInfo_open (no KDU, no FMOD)
```

To build firestorm:

```
autobuild build -c ReleaseFS                        
```

Other examples:

```
autobuild configure -c ReleaseFS                              # basic configuration step, don't build, just configure
autobuild configure -c ReleaseFS -- --clean                   # clean the output area first, then configure
autobuild configure -c ReleaseFS -- --chan Private-Yourname   # configure with a custom channel

autobuild build -c ReleaseFS --no-configure                   # default quick rebuild
autobuild build -c ReleaseFS --no-configure -- --clean        # Clean rebuild

autobuild configure -c ReleaseFS_open --                      # configure with no third-party libraries
autobuild configure -c ReleaseFS_open -- --fmodstudio         # configure with FMOD Studio but no KDU
```

Any of the configure options can also be used (and do the same thing) with the build options. Typical LL autobuild configure options should also work, as long as they don't duplicate configuration we are already doing.

Logs: Look for logs in `build-darwin-x86_64/logs`.

Output: Look for output in `build-darwin-x86_64/newview/Release`.

##  Set up your source code tree

Plan your directory structure ahead of time. If you are going to be producing changes or patches you will be cloning a copy of an unaltered source code tree for every change or patch you make, so you might want to have all this work stored in its own directory. If you are a casual compiler and won't be producing any changes, you can use one directory. For this document, we will assume $HOME/firestorm.

```
mkdir ~/firestorm
cd ~/firestorm
git clone https://github.com/FirestormViewer/phoenix-firestorm.git
```

This can take a while. It's a rather large download.

You will also need to download the build variables used for building the viewer. Like the viewer source, these are downloaded from the Firestorm git repository. Assuming you are still in your ~/firestorm directory (or wherever else you chose), issue the following commands:

```
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

You will then need to add this line to your ~/.zshrc file (assuming you are on Catalina or later):
```
echo 'AUTOBUILD_VARIABLES_FILE=~/firestorm/fs-build-variables/variables' | sudo tee ~/.zshrc
```

Again, if you do not wish to restart your terminal:
```
export AUTOBUILD_VARIABLES_FILE=~/firestorm/fs-build-variables/variables
```

##  Prepare third party libraries

Most third party libraries needed to build the viewer will be automatically downloaded for you and installed into the build directory within your source tree during compilation. Some need to be manually prepared and are not normally required when using an open source configuration (ReleaseFS_open).

### FMOD Studio using autobuild 

- Get the FMOD Studio API disk image installer for the Mac from the [FMOD site](https://www.fmod.com) (requires creating an account to access the download section).

> [!IMPORTANT]
> Make sure to download the FMOD Studio API and not the FMOD Studio Tool!

- Enter these commands into the terminal window:

```
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
cd 3p-fmodstudio
```

- Place the installer disk image (.dmg) file you downloaded into the current directory.
- Issue the following commands:
```
autobuild build
autobuild package --results-file result.txt
```

Near the top of the output you will see the package name written:

```
wrote  /Users/yourname/3p-fmodstudio/fmodstudio-2.01.05-darwin-202981448.tar.bz2
```

Additionally, a file `result.txt` has been created containing the md5 hash value of the package file, which you will need in the next step.

- Next, update Firestorms autobuild.xml file to use your FMOD Studio.

```
cd ~/firestorm/phoenix-firestorm
cp autobuild.xml my_autobuild.xml
export AUTOBUILD_CONFIG_FILE=my_autobuild.xml
```

- Copy the fmodstudio path and md5 value from the package process into this command:

```
autobuild installables edit fmodstudio platform=darwin64 hash=<md5 value> url=file:///<fmod path>
```

For example:
```
autobuild installables edit fmodstudio platform=darwin64 hash=3b0d38f2a17ff1b73c8ab6dffbd661eb url=file:///Users/yourname/3p-fmodstudio/https://fmodstudio-2.01.05-darwin-202981448.tar.bz2
```
> [!NOTE]
> Having to copy autobuild.xml and modify the copy from within a cloned repository is a lot of work for every repository you make, but this is the only way to guarantee you pick up upstream changes to autobuild.xml and do not send up a modified autobuild.xml when you do a git push.

## Configuring the Viewer

The following is all still done from within the terminal window:
```
cd ~/firestorm/phoenix-firestorm
autobuild configure -A 64 -c ReleaseFS_open
```

This will configure the viewer for compiling with all defaults and without third party libraries.

###  Configuration Switches

There are a number of switches you can use to modify the configuration process.  The name of each switch is followed by its type and then by the value you want to set.

- FMODSTUDIO (bool) controls if the FMOD Studio package is incorporated into the viewer. You must have performed the FMOD Studio installation steps in [FMOD Studio using autobuild](#fmod-studio-using-autobuild) for this to work. This is the switch the --fmodstudio build argument sets.
- LL_TESTS (bool) controls if the tests are compiled and run. There are quite a lot of them so excluding them is recommended unless you have some reason to need one or more of them.

> [!TIP]
> OFF and NO are the same as FALSE; anything else is considered to be TRUE

Example:

```
autobuild configure -A 64 -c ReleaseFS_open -- -DLL_TESTS:BOOL=FALSE -DFMODSTUDIO:BOOL=TRUE
```
```
autobuild configure -A 64 -c ReleaseFS_open
```

> [!IMPORTANT]
> You must specify the -A 64 switch to autobuild whenever you configure or build. This tells autobuild to build a 64-bit viewer; this is the only architecture supported on macOS.

##  Compiling the Viewer

To compile the code into an app that can be used on your computer, run the following in the terminal window:

```
cd ~/firestorm/phoenix-firestorm
autobuild build -A 64 -c ReleaseFS_open --no-configure
```

Compiling may take quite a bit of time, depending on how fast your machine is and how much else you're doing.

> [!NOTE]
> It is possible to use autobuild to do both the configure step (only needed once) and the build step with one command (`autobuild build -A 64 -c ReleaseFS[_open] [-- config options]`). Some find it is clearer if these steps are done separately, but can save a bit of time if done together.

## Launching the viewer

Now that the viewer has been compiled, it can be found in `build-darwin-x86_64/newview/Release`.  Enter the following into the terminal to open Finder:

```
open build-darwin-x86_64/newview/Release
```

From here you can run it directly or copy it to the Applications folder for ease of finding and running later.

## Updating the viewer

If you want to update your self-compiled viewer, you don't have to go through this entire page again. Follow these steps to pull down any new code and re-compile.

- First you need to change to the directory where you built the viewer before. Assuming you followed the steps above and used the same names, run the following in the terminal window: `cd ~/firestorm/phoenix-firestorm`
- Next, decide if you are going to remove the already compiled files and perform a "Clean build", or if you want to keep the existing build files, "Dirty build". Clean builds are generally recommended by developers but have the downside of building everything again, which comes at a cost of time; dirty builds on the other hand, keep all the previously compiled files and only compile the parts that have changed. The downside to dirty builds comes when a change to a core file is pulled down, which will then cause everything to be rebuilt, negating any speed benefit or possibly taking longer than a clean build.
- If you have picked to do a clean build, run the following in the terminal window, otherwise skip to the next step: `rm -r build-darwin-x86_64`
- If you are using a custom autobuild.xml file, then run the following in the terminal window, otherwise skip this step: `export AUTOBUILD_CONFIG_FILE=my_autobuild.xml`
- Now, to pull down any new code, run the following in the terminal window: `git pull`
- After any new code is downloaded, it needs to be reconfigured. Run the following in the terminal window: `autobuild configure -A 64 -c ReleaseFS_open`
- Finally, re-compile the viewer with the new changes. Again, in the terminal window, run: `autobuild build -A 64 -c ReleaseFS_open --no-configure`

Finally, follow the instructions in [Launching the viewer](#launching-the-viewer) to run your freshly updated viewer.
