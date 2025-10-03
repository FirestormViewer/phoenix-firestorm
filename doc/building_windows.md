# Build instructions for Windows

This page describes all necessary steps to build the Firestorm viewer for Windows. For building instructions up to (and including) release 6.5.3, see the archived version for [building with Python 2.7](https://wiki.firestormviewer.org/archive:fs_compiling_firestorm_windows_py_27).

> [!WARNING]
> Please note that we do not give support for compiling the viewer on your own. However, there is a self-compilers group in Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

> [!IMPORTANT]
> With the [merge of Linden Lab release 6.6.16](https://github.com/FirestormViewer/phoenix-firestorm/commit/b64793e2b0d14e44274335c874660af9f679f7f8) it is **NOT** possible to create 32bit builds anymore! Only 64bit builds are possible going forward!

## Install required development tools

This is needed for compiling any viewer based on the Linden Lab open source code and only needs to be done once.

All installations are done with default settings (unless told explicitly) - if you change that, you're on your own!

### Windows

- Install Windows 10/11 64bit using your own product key

### Microsoft Visual Studio 2022

- Install Visual Studio 2022
  - Run the installer as Administrator (right click, "Run as administrator")
  - Check "Desktop development with C++" on the "Workloads" tab.
  - All other workload options can be unchecked

> [!TIP]
> If you don't own a copy of a commercial edition of Visual Studio 2022 (e.g. Professional), you might consider installing the [Community version](https://visualstudio.microsoft.com/free-developer-offers)

### Command Prompt vs Powershell
- Make sure that you use the Windows Command Prompt / Terminal (cmd.exe) and not Powershell or it won't detect the Visual Studio build tools properly.

### Git

-  If you prefer having a GUI for Git, download and install [TortoiseGit 2.17.0 or newer](https://tortoisegit.org) (64bit)
  - Note: No option available to install as Administrator
  - Use default options (path, components etc.) for Tortoise Git itself
  - At some point, it will ask you to download and install Git for Windows
    - You can install with default options **EXCEPT** when it asks for "Configuring the line endings conversion": You **MUST** select "Checkout as-is, commit as-is" here!

- If you prefer command line tools, you can also use and install the official [Git for Windows](https://git-scm.com/downloads/win).
  - Uncheck "associate .sh files to be run with bash"
  - Choose a good text editor such as VS Code or Sublime Text when asked to
  - Select "Let Git decide the name of the initial branch"
  - Select "Git from the command line and also from 3rd party software"
  - Select "Use External SSH"
  - Select "Use the native Windows Secure Channel library"
  - Select "Checkout as-is, commit as-is" for line endings conversion.
  - Select "Use Windows' default console window"
  - Select "Fast-forward or merge"
  - Select "Git Credential Manager"
  - Check "Enable file system caching"
  - Once installed, ensure that the git directory (C:\Program Files\Git\cmd) is in your PATH.

### CMake

- Download and install at least [CMake 3.16.0](http://www.cmake.org/download)
  - Note: No option available to install as Administrator
  - At the "Install options" screen, select "Add CMake to the system PATH for all users"
  - For everything else, use the default options (path, etc.)
  - Make sure that the following directory was added to your path:
    For the 32bit version:
    `C:\Program Files (x86)\CMake\bin`
    For the 64bit version:
    `C:\Program Files\CMake\bin`

### Cygwin

- Download and install [Cygwin 64](http://cygwin.com/install.html) (64bit)
  - Run the installer as Administrator (right click, "Run as administrator")
  - Use default options (path, components etc.) *until* you get to the "Select Packages" screen
  - Add additional packages:
    - Devel/patch
  - Use default options for everything else
  - Make sure that the following directory was added to your path and that it is placed before "%SystemRoot%\system32" but after CMake path (in case you installed CMake in Cygwin):
    `C:\Cygwin64\bin`

> [!NOTE]
> The Cygwin terminal is only needed for testing. All commands for actually building the viewer will be run from the Windows command shell.

### Python

- Download and install the most recent version of [Python 3](https://www.python.org/downloads/windows)
  - Run the installer as Administrator (right click, “Run as administrator”)
  - Tick "Add Python 3.10 to PATH"
  - Choose the "Customize Installation" option.
    - Make sure that "pip" is ticked.
    - "Documentation", "tcl/tk and IDLE", "Python test suite" and "py launcher" are not needed to compile the viewer but can be selected if you wish.
    - On the next screen, the correct options should already be ticked.
    - Set custom install location to: `C:\Python3`
  - Make sure that the following directory was added to your path: `C:\Python3`

> [!TIP]
> On Windows 10/11, you also might want to disable the app alias for Python. Open the Windows settings app, search for "Manage app execution aliases" and disable the alias for "python3.exe"

### Intermediate check

Confirm things are installed properly so far by opening a Cygwin terminal and enter:

```
cmake --version
git --version
python --version
pip --version
```

If they all report sensible values and not "Command not found" errors, then you are in good shape.

### Optional: Set up a Python virtual environment

If you do not want to install the required Python packages into the default Python directory, you can optionally create a virtual environment.

- Create the Python Virtual Environment (only once). Open Windows Command Prompt and navigate to the directory where you want to install the virtual environment. To create the virtual environment, enter:
  `python -m venv <env_name>` (`<env_name>` is a placeholder for the name you want to use for the virtual environment)
- Activate the virtual environment by typing:
  `<env_name>\Scripts\activate.bat`
- Activate the virtual environment each time you want to build.
- Type all the subsequent commands in this virtual environment.
- In case of issue or Python update, you can delete this directory for the virtual environemt and create a new one again.

### Set up Autobuild

- Install Autobuild
   You can install autobuild and its dependencies using the `requirements.txt` file that is part of the repo, this will build using the same versions that our official builds use.
  - Open Windows Command Prompt
  - If you created a Python virtual environment earlier, activate it
  - Enter: <code>pip install -r requirements.txt</code>
  - Autobuild will be installed. **Earlier versions of Autobuild could be made to work by just putting the source files into your path correctly; this is no longer true - Autobuild _must_ be installed as described here.**
  - Open Windows Command Prompt and enter:
    `pip install git+https://github.com/secondlife/autobuild.git#egg=autobuild`
- Set environment variable AUTOBUILD_VSVER to 170 (170 = Visual Studio 2022).
- Check Autobuild version to be "autobuild 3.8" or higher:
  `autobuild --version`

### NSIS

- If you plan to package the viewer and create an installer file, you must install the NSIS from the [official website](https://nsis.sourceforge.io).
- Not required unless you need to build an actual viewer installer for distribution, or change the NSIS installer package logic itself
  
> [!IMPORTANT]
> If you want to package the viewer built on a revision prior to the [Bugsplat merge](https://github.com/FirestormViewer/phoenix-firestorm/commit/a399c6778579ac7c8965737088c275dde1371c9e), you must install the Unicode version of NSIS [from here](http://www.scratchpaper.com) - the installer from the NSIS website **WILL NOT** work!

## Setup viewer build variables

In order to make it easier to build collections of related packages (such as the viewer and all the library packages that it imports) with the same compilation options, Autobuild expects a file of variable definitions. This can be set using the environmenat variable AUTOBUILD_VARIABLES_FILE.

- Clone the build variables repository: 
  `git clone https://github.com/FirestormViewer/fs-build-variables.git <path-to-your-variables-file>`
- Set the environment variable AUTOBUILD_VARIABLES_FILE to
  `<path-to-your-variables-file>\variables`

## Configure Visual Studio 2022 (optional)

- Start the IDE
- Navigate to **Tools** > **Options** > **Projects and Solutions** > **Build and Run** and set **maximum number of parallel projects builds** to **1**.

## Set up your source code tree

Plan your directory structure ahead of time. If you are going to be producing changes or patches you will be cloning a copy of an unaltered source code tree for every change or patch you make, so you might want to have all this work stored in its own directory. If you are a casual compiler and won't be producing any changes, you can use one directory. For this document, it is assumed that you created a folder c:\firestorm.

```
c:
cd \firestorm
git clone https://github.com/FirestormViewer/phoenix-firestorm.git
```

## Prepare third party libraries

Most third party libraries needed to build the viewer will be automatically downloaded for you and installed into the build directory within your source tree during compilation. Some need to be manually prepared and are not normally required when using an open source configuration (ReleaseFS_open). Some libraries like Kakadu or Havok requires you to purchase a license and you will need to figure out yourself how to build and use them.

> [!IMPORTANT]
> If you are manually building the third party libraries, you will have to build the correct version (32bit libraries for a 32bit viewer, 64bit versions for a 64bit viewer)!

## FMOD Studio using Autobuild

If you want to use FMOD Studio to play sounds within the viewer, you will have to download your own copy. FMOD Studio can be downloaded [here](https://www.fmod.com) (requires creating an account to access the download section).

> [!IMPORTANT]
> Make sure to download the FMOD Studio API and not the FMOD Studio Tool!

```
c:
cd \firestorm
git clone https://github.com/FirestormViewer/3p-fmodstudio.git
```

- After you have cloned the repository, copy the downloaded FMOD Studio installer file into the root of the repository
- Make sure to modify the file build-cmd.sh in the root of the repository and set the correct version number based on the version you downloaded. Right at the top, you find the version number of FMOD Studio you want to package (one short version without separator and one long version):

```
FMOD_VERSION="20102"
FMOD_VERSION_PRETTY="2.01.02"
```

Continue on the Windows command line:

```
c:
cd \firestorm\3p-fmodstudio
autobuild build -A 64 --all
autobuild package -A 64 --results-file result.txt
```

While running the Autobuild build command, Windows might ask if you want to allow making changes to the computer. This is because of the FMOD Studio installer being executed. Allow these changes to be made.

Near the end of the output you will see the package name written:

```
wrote  C:\firestorm\3p-fmodstudio\fmodstudio-{version#}-windows64-{build_id}.tar.bz2''
```

where {version#} is the version of FMOD Studio (like 2.01.02) and {build_id} is an internal build id of the package. Additionally, a file `result.txt` has been created containing the md5 hash value of the package file, which you will need in the next step.

```
cd \firestorm\phoenix-firestorm
cp autobuild.xml my_autobuild.xml
set AUTOBUILD_CONFIG_FILE=my_autobuild.xml
```

Copy the FMOD Studio path and md5 value from the package process into this command:

`autobuild installables edit fmodstudio platform=windows64 hash=<md5 value> url=file:///<fmodstudio path>`

For example:

`autobuild installables edit fmodstudio platform=windows64 hash=a0d1821154e7ce5c418e3cdc2f26f3fc url=file:///C:/firestorm/3p-fmodstudio/fmodstudio-2.01.02-windows-192171947.tar.bz2`

> [!NOTE]
> Having to copy autobuild.xml and modify the copy from within a cloned repository is a lot of work for every repository you make, but this is the only way to guarantee you pick up upstream changes to autobuild.xml and do not send up a modified autobuild.xml when you do a git push.

## Configuring the viewer

Open the Windows command prompt.

If you are building with FMOD Studio and have followed the previous FMOD Studio setup instructions AND you are now using a new terminal you will need to reset the environment variable first by entering 

`set AUTOBUILD_CONFIG_FILE=my_autobuild.xml`

Then enter:

```
c:
cd \firestorm\phoenix-firestorm
autobuild configure -A 64 -c ReleaseFS_open
```

This will configure Firestorm to be built with all defaults and without third party libraries.

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

> [!TIP]
> Configuring the viewer for the first time will take some time to download all the required third-party libraries. The download progress is hidden by default. If you want to watch the download progress, you can use the verbose option to display a more detailed output:
> `autobuild configure -A 64 -v -c ReleaseFS_open`

### Configuration switches

There are a number of switches you can use to modify the configuration process. The name of each switch is followed by its type and then by the value you want to set.

- **-A \<architecture\>** sets the target architecture, that is if you want to build a 32bit or 64bit viewer (32bit is default if omitted). You probably want to set this to `-A 64`.
- **--avx** will enable AVX optimizations for AVX-enabled CPUs. Mutually exclusive with --avx2.
- **--avx2** will enable AVX2 optimizations for AVX2-enabled CPUs. Mutually exclusive with --avx.
- **--clean** will cause autobuild to remove any previously compiled objects and fetched packages. It can be useful if you need to force a reload of all packages.
- **--fmodstudio** controls if the FMOD Studio package is incorporated into the viewer. You must have performed the FMOD Studio installation steps in [FMOD Studio using Autobuild](#fmod-studio-using-autobuild) for this to work. You will not have any sound if you do not include FMOD.
- **--kdu** will tell autobuiild to use the KDU (Kakadu) package when compiling.
- **--package** makes sure all files are copied into viewers output directory. You won't be able to start your compiled viewer if you don't enable package or do 'compile' it in VS. It will also run NSIS to create a setup package.
- **--chan \<channel name\>** will set a unique channel (and the name) for the viewer, appending whatever is defined to "Firestorm-". By default, the channel is "private" followed by your computer's name.
- **-LL_TESTS:BOOL=\<bool\>** controls if the tests are compiled and run. There are quite a lot of them so excluding them is recommended unless you have some reason to need one or more of them.

> [!TIP]
> **OFF** and **NO** are the same as **FALSE**; anything else is considered to be **TRUE**

### Examples: ###

- To build a 64bit viewer with FMOD Studio and to create an installer package, run this command in the Windows command window:
`autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio --package --chan MyViewer -DLL_TESTS:BOOL=FALSE`

- To build a 64bit viewer without FMOD Studio and without installer package, run this command:
`autobuild configure -A 64 -c ReleaseFS_open -- --chan MyViewer -DLL_TESTS:BOOL=FALSE`

## Building the viewer

There are two ways to build the viewer: Via Windows command line or from within Visual Studio.

### Building from the Windows command line

Make sure that you are using the Windows Command Prompt / Terminal (cmd.exe) and not Powershell.

If you are building with FMOD Studio and have followed the previous FMOD Studio setup instructions AND you are now using a new terminal you will need to reset the environment variable with 

`set AUTOBUILD_CONFIG_FILE=my_autobuild.xml`

Then run the Autobuild build command. Make sure you include the same architecture parameter you used while [configuring the viewer](#configuring-the-viewer):

`autobuild build -A 64 -c ReleaseFS_open --no-configure`

Compiling will take quite a bit of time.

### Building from within Visual Studio

Inside the Firestorm source folder, you will find a folder named build-vc170-\<architecture\>, with \<architecture\> either being 32 or 64, depending on what you chose during the configuration step. Inside the folder is the Visual Studio solution file for Firestorm, called Firestorm.sln.

- Double-click Firestorm.sln to open the Firestorm solution in Visual Studio.
- From the menu, choose Build -> Build Solution
- Wait until the build is finished

## Troubleshooting

### SystemRootsystem32: unbound variable

When trying to execute the Autobuild build command, you might encounter an error similar to

`../build.cmd.sh line 200: SystemRootsystem32: unbound variable`

This error is caused by the order of the items in the Windows "path" environment variable. Autobuild exports all paths set in the "path" environment variable into Cygpath names and variables. Since these Windows "paths" can also contain variables like %SystemRoot% and they can also depend on each other, it is important to keep the dependency order intact. Example:

```
%SystemRoot%
%SystemRoot%\system32
%SystemRoot%\System32\Wbem
```

Make sure the ones mentioned are the first items set in the "path" environment variable.
