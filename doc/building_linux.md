# Build instructions for Linux

> [!WARNING]
> Please note that we do not give support for compiling the viewer on your own. However, there is a self-compilers group in Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

This procedure is based on discussions with the Firestorm Linux development team and is the only one recommended for Firestorm for Linux. System requirements are:
- Ubuntu 22.04 LTS (x86_64) - fully upgraded (this is also now the minimum requirement for running the viewer).
- 16GB or more RAM ([Low Memory Caution](#common-issuesbugsglitches-and-solutions))
- 64GB hard drive space 
- 4 or more core CPU (you could get by with 2 cores, but the process will take much longer)
- GCC 11 compiler (which is the default version on Ubuntu 22.04 LTS)

It is recommended that you use a virtual machine for compiling, ensuring the guest can meet the hardware requirements.

This procedure may or may not work on other Linux distributions (or you might need to adjust some of the package names to suit the distribution you are using).

> [!WARNING]
> A system with a glibc version of at least 2.34 is required (Ubuntu 22.04 LTS meets this requirement) 
> Building on a system with a glibc version older than 2.34 will likely result in linker errors.

> [!IMPORTANT]
> Only 64 bit builds are possible - 32 bit support was dropped quite some time ago.

## Establish your programming environment

This only needs to be done once.

### Create your source tree

Typically, Linux source code is stored in a src directory in your home directory. So: `mkdir ~/src`

### Install required packages

A few packages must be installed on the build system. Some may already be installed:

|                 |                  |               |                    |                  |        |
| --------------- | ---------------- | ------------- | ------------------ | ---------------- | ------ |
| libgl1-mesa-dev | libglu1-mesa-dev | libpulse-dev  | build-essential    | python3-pip      | git    |
| libssl-dev      | libxinerama-dev  | libxrandr-dev | libfontconfig-dev  | libfreetype6-dev | gcc-11 |
| cmake           |                  |               |                    |                  |        |

```
sudo apt install libgl1-mesa-dev libglu1-mesa-dev libpulse-dev build-essential python3-pip git libssl-dev libxinerama-dev libxrandr-dev libfontconfig-dev libfreetype6-dev gcc-11 cmake
```

### Optional: Set up a Python virtual environment

If you do not want to install the required Python packages into the default Python directory, you can optionally create a virtual environment.

- Create the Python virtual environment (only once):
  `python -m venv .venv`
- Activate the virtual environment:
  `source .venv/bin/activate`
- Activate the virtual environment each time you want to build.
- Type all the subsequent commands in this virtual environment.
- In case of issue or Python update, you can delete this .venv directory and create a new virtual environment again.

### Install Autobuild

Autobuild is a Linden Lab resource that does all the hard work.
If you created a Python virtual environment, activate it first.
You can install it using the same versions as our automated builds as follows:
```
sudo pip3 install --upgrade pip
pip install -r requirements.txt
```
Check Autobuild version to be "autobuild 3.9.3" or higher: `autobuild --version`
## Download the source code

There are two required repositories, the viewer itself and the build variables. An optional third repository is used to configure and package FMOD Studio.

### Clone the viewer

```
cd ~/src
git clone https://github.com/FirestormViewer/phoenix-firestorm.git
```

This will create a folder called phoenix-firestorm and add all the source files. If you desire, you can choose a different folder name by adding the name to the end of the command:

```
git clone https://github.com/FirestormViewer/phoenix-firestorm.git NewDestinationDirectory
```
 
The rest of this document will assume the default directory, `phoenix-firestorm`

### Clone the Autobuild build variables

Autobuild uses a separate file to control compiler options, switches, and the like for different configurations. 

```
cd ~/src
git clone https://github.com/FirestormViewer/fs-build-variables.git
```

### Create FMOD Studio package (optional)

Although not required, including FMOD Studio in your build will improve your audio-based experience.

```
cd ~/src
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
cd ~/src/3p-fmodstudio
```

Open the file called `build-cmd.sh` and look at the fifth line down, it begins with `FMOD_VERSION`. This is the version of the API you need to download.

The FMOD Studio API can be downloaded [here](https://www.fmod.com) (requires creating a free account to access the download section).

Click the button representing the version you're after, then click the Download link for the Linux file.

> [!IMPORTANT]
> Make sure to download the FMOD Studio API and not the FMOD Studio Tool!

Copy that file to the `~/src/3p-fmodstudio` directory.

```
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
```

Near the end of the output you will see the package name written, similar to:

```
wrote  !/home/username/src/3p-fmodstudio/fmodstudio-2.01.02-linux64-202161533.tar.bz2
```

Additionally, a file `result.txt` has been created containing the md5 hash value of the package file, which you will need in the next steps.

```
cd ~/src/phoenix-firestorm
```

Copy the FMOD Studio path and md5 value from the package process into this command:

```
autobuild installables edit fmodstudio platform=linux64 hash=<md5 value> url=file:///<fmodstudio path>
```

For example:

```
autobuild installables edit fmodstudio platform=linux64 hash=c3f696412ef74f1559c6d023efe3a087 url=file:///!/src/3p-fmodstudio/fmodstudio-2.00.07-linux64-200912220.tar.bz2
```

> [!NOTE]
> Having modified autobuild.xml would require it be restored before trying to fetch any new commits or more especially if you push a commit. This can be done with `git reset --hard && git remote update`

## Configure and build

### Configuring the viewer

Start by initializing the variables

```
export AUTOBUILD_VARIABLES_FILE=$HOME/src/fs-build-variables/variables
```

You can add that to `~/.bashrc` or `~/.profile` so they execute automatically, or execute them before you run autobuild.

```
cd ~/src/phoenix-firestorm
autobuild configure -A 64 -c ReleaseFS_open
```

This will set up to compile with all defaults and without non-default libraries. It will fetch any additional necessary libraries.

Available premade firestorm-specific build targets:

```
ReleaseFS             (with KDU, with FMOD,   no OpenSim)
ReleaseFS_AVX         (with KDU, with FMOD,   no OpenSim, optimized for AVX-enabled CPUs)
ReleaseFS_AVX2        (with KDU, with FMOD,   no OpenSim, optimized for AVX2-enabled CPUs)
ReleaseFS_open        (  no KDU,   no FMOD,   no OpenSim)
ReleaseOS             (  no KDU,   no FMOD, with OpenSim)
RelWithDebInfoFS      (with KDU, with FMOD,   no OpenSim, with debug info)
RelWithDebInfoFS_open (  no KDU,   no FMOD,   no OpenSim, with debug info)
RelWithDebInfoOS      (  no KDU,   no FMOD, with OpenSim, with debug info)
```

### Configuration Switches

There are a number of switches you can use to modify the configuration process.  The name of each switch is followed by its type and then by the value you want to set.

- **-A \<architecture\>** sets the target architecture, that is if you want to build a 32bit or 64bit viewer (32bit is default if omitted). You probably want to set this to `-A 64`.
- **--avx** will enable AVX optimizations for AVX-enabled CPUs. Mutually exclusive with --avx2.
- **--avx2** will enable AVX2 optimizations for AVX2-enabled CPUs. Mutually exclusive with --avx.
- **--clean** will cause autobuild to remove any previously compiled objects and fetched packages. It can be useful if you need to force a reload of all packages
- **--fmodstudio** will tell autobuiild to use the FmodStudio package when compiling.
- **--kdu** will tell autobuiild to use the KDU (Kakadu) package when compiling.
- **--package** makes sure all files are copied into viewers output directory. It will also result in a bzip2 archive of the completed viewer. Enabled by default, you would have to use **-DPACKAGE:BOOL=Off** to disable it
- **--chan \<channel name\>** will set a unique channel (and the name) for the viewer, appending whatever is defined to "Firestorm-". By default, the channel is "private" followed by your computer's name.
- **-LL_TESTS:BOOL=\<bool\>** controls if the tests are compiled and run. There are quite a lot of them so excluding them is recommended unless you have some reason to need one or more of them.

Most switches start with a double-dash (\--). And if you use any switches you must enclose them with a double-dash at the start and an optional double-dash at the end.

> [!TIP]
> **OFF** and **NO** are the same as **FALSE**; anything else is considered to be **TRUE**

### Examples: ###

```
autobuild configure -A 64 -c ReleaseFS_open -- -DLL_TESTS:BOOL=FALSE
autobuild configure -A 64 -c ReleaseFS_open -- --clean
autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio
autobuild configure -A 64 -c ReleaseFS_open -- --chan="MyBuild"
```

In the last example, the channel and resulting viewer name would be "Firestorm-MyBuild". 

The first time you configure, several additional files will be downloaded from Firestorm and Second Life sources. These are mostly binary packages maintained outside the viewer development itself. And if you use the `--clean` switch, you will re-download them all.

## Compiling the viewer

```
autobuild build -A 64 -c ReleaseFS_open
```

Be sure to use the fmodstudio and chan switches again.

Compiling can take quite a bit of time depending on your computer's processing power.

> [!NOTE]
> It is possible to use autobuild to do both the configure step (only needed once) and the build step with one command (`autobuild build -A 64 -c ReleaseFS_open -- --clean [more switches]`). For clarity, they are mentioned separately.

> [!TIP]
> When using the --package switch you can set the XZ_DEFAULTS variable to -T0 to use all available CPU cores to create the .tar.xz file. This can significantly reduce the time needed to create the archive, but it will use a lot more memory. For example:
> ```
> export XZ_DEFAULTS="-T0"
> autobuild build -A64 -c ReleaseFS_open -- --package
> ```

### Copy out of the guest

If you build the viewer using a virtual machine (guest), you will need to copy the viewer files over to your host or to a different machine in order to use the viewer. Your guest may not have sufficient resources to run the viewer. The rsync program can make copying easy and accurate. And if you build the viewer again, the options included in the command example will cause rsync to only copy the new files, cutting down on the time needed to copy.

The build process created a ready-to-use viewer as well as a compressed archive. The archive can be copied or moved to any shared filesystem, such as a flash or cloud drive, and it could be installed or extracted in the same manner as is the official release.

```
cp ~/src/phoenix-firestorm/build-linux-x86_64/newview/Phoenix*.tar.* /path/to/shared/drive
```

or
```
mv ~/src/phoenix-firestorm/build-linux-x86_64/newview/Phoenix*.tar.* /path/to/shared/drive
```

When copying the ready-to-run folders and files, use

```
rsync -rptgoDLK --update --progress ~/src/phoenix-firestorm/build-linux-x86_64/newview/packaged/* /path/to/destination
```

Using rsync has the advantage of updating the destination, replacing only those files that changed or are missing, which takes much less time than copying and replacing every file.

## Running your newly built viewer

### Running from a menu item

Create the desktop launcher after copying to the destination machine

```
cd /path/to/firestorm
etc/refresh_desktop_app_entry.sh
```

Then open your applications menu and look in the Internet or Network branch for the Firestorm launcher.

### Running from command line or file browser

```
cd /path/to/firestorm
./firestorm
```

## Troubleshooting

### Handling problems

If you encounter errors or run into problems, please first double check that you followed the steps correctly. One typo can break it. Then, check whether someone else already had the same issue. A solution might be known already.

- **Firestorm Self-Compilers group:** [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers) is free to join, fellow self-compilers may be able to offer assistance.
- **Jira:** [JIRA](https://jira.firestormviewer.org) may contain resolved issues related to the error you're seeing. Search using the error you encountered. Or create a new issue to report an error in this document, or if a code change causes a build process to fail.

### Common issues/bugs/glitches and solutions

- **Virtual memory exhausted, c++ fatal error, or similar out-of-memory issues** may occur if you are building the viewer for the first time, or re-building after a very large set of changes were added. Sometimes restarting the build command will let you recover, sometimes you have to restart the build system. To avoid that from happening, add ram to your virtual environment, or add swap space, something on the order of 10GB or more. As well, reducing the number of CPU cores assigned, down to 4 or less, will lower ram usage.
- **Missing libraries/applications/packages:** This may occur if you did not or could not install the packages shown above, or are attempting to use this procedure with a different Linux OS, or are attempting to build a 32-bit viewer. Start over, making sure you are using the right OS, reinstall all packages as listed, and do not attempt to make a 32-bit viewer.
- **SDL2:** Currently the SDL2 install/update process has a problem if the SDL2 files already exist. The workaround is to delete those files:
```
rm -rf ../build-linux-x86_64/packages/include/SDL2/
rm -rf ../build-linux-x86_64/packages/docs/SDL/
rm ../build-linux-x86_64/packages/LICENSES/SDL.txt
rm ../build-linux-x86_64/packages/lib/release/*SDL*
```
- **Delayed sounds:** Some users have noted that OpenAL plays sounds from the viewer up to 20 seconds after they are triggered. There is no solution to this via the viewer, but there may be some solutions on the Internet. Compiling with FModStudio may resolve this issue.
- **No sounds:** The viewer will try to use whatever sound service you have running, but might need a little coaxing. Read through the firestorm script inside the program directlry, you will find various commented options. Uncommenting one or more may help restore sound, as can compiling with FModStudio. Refer also to the README.Linux.txt and README-linux-voice.txt files in the program directory. 
- **Voice won't connect:** Refer to **[this link](https://wiki.firestormviewer.org/fs_voice#linux)** or the relevant link on **[this page](https://wiki.firestormviewer.org/linux)** to make needed adjustments to your computer and/or the SLVoice files.
