@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1

set "BOOST_ROOT=C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0"
set "ENGINE_DIR=%BOOST_ROOT%\tools\build\src\engine"

echo === Step 1: Compile b2.exe ===
cd /d "%ENGINE_DIR%"

set "CXX=cl"
set "B2_CXX=%CXX% /nologo /MP /MT /TP /Feb2 /wd4996 /wd4675 /O2 /GL /EHsc /Zc:wchar_t /Gw"
set "B2_CXX_LINK=/link kernel32.lib advapi32.lib user32.lib /MANIFEST:EMBED /MANIFESTINPUT:b2.exe.manifest"

set B2_SOURCES=bindjam.cpp builtins.cpp class.cpp command.cpp compile.cpp constants.cpp cwd.cpp
set B2_SOURCES=%B2_SOURCES% debug.cpp debugger.cpp events.cpp
set B2_SOURCES=%B2_SOURCES% execcmd.cpp execnt.cpp execunix.cpp filent.cpp filesys.cpp fileunix.cpp frames.cpp function.cpp
set B2_SOURCES=%B2_SOURCES% glob.cpp hash.cpp hcache.cpp hdrmacro.cpp headers.cpp jam.cpp
set B2_SOURCES=%B2_SOURCES% jamgram.cpp lists.cpp make.cpp make1.cpp md5.cpp mem.cpp modules.cpp
set B2_SOURCES=%B2_SOURCES% native.cpp option.cpp output.cpp parse.cpp pathnt.cpp
set B2_SOURCES=%B2_SOURCES% pathsys.cpp pathunix.cpp regexp.cpp rules.cpp scan.cpp search.cpp jam_strings.cpp
set B2_SOURCES=%B2_SOURCES% startup.cpp tasks.cpp
set B2_SOURCES=%B2_SOURCES% timestamp.cpp value.cpp variable.cpp w32_getreg.cpp
set B2_SOURCES=%B2_SOURCES% mod_command_db.cpp mod_db.cpp mod_jam_builtin.cpp mod_jam_class.cpp
set B2_SOURCES=%B2_SOURCES% mod_jam_errors.cpp mod_jam_modules.cpp mod_order.cpp mod_path.cpp
set B2_SOURCES=%B2_SOURCES% mod_property_set.cpp mod_regex.cpp mod_sequence.cpp mod_set.cpp
set B2_SOURCES=%B2_SOURCES% mod_string.cpp mod_summary.cpp mod_sysinfo.cpp mod_version.cpp

set B2_CXXFLAGS=-DNDEBUG

%B2_CXX% %B2_CXXFLAGS% %B2_SOURCES% %B2_CXX_LINK%
if errorlevel 1 (
    echo Failed to compile b2.exe
    exit /b 1
)
echo b2.exe built successfully

copy "%ENGINE_DIR%\b2.exe" "%BOOST_ROOT%\b2.exe" >nul

echo === Step 2: Generate project-config.jam ===
cd /d "%BOOST_ROOT%"

echo import option ; > project-config.jam
echo using msvc : 14.3 ; >> project-config.jam
echo option.set keep-going : false ; >> project-config.jam

echo === Step 3: Build Boost libraries for ARM64 ===
"%BOOST_ROOT%\b2.exe" ^
    --with-atomic ^
    --with-chrono ^
    --with-container ^
    --with-context ^
    --with-date_time ^
    --with-fiber ^
    --with-filesystem ^
    --with-iostreams ^
    --with-json ^
    --with-program_options ^
    --with-regex ^
    --with-stacktrace ^
    --with-system ^
    --with-thread ^
    --with-url ^
    --with-wave ^
    toolset=msvc ^
    architecture=arm ^
    address-model=64 ^
    variant=release ^
    link=static ^
    runtime-link=shared ^
    threading=multi ^
    context-impl=winfib ^
    --layout=tagged ^
    -j8 ^
    stage

echo === Build complete (exit code: %errorlevel%) ===
