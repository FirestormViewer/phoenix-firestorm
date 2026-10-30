"""Configuration regressions for the platform-specific crash-reporting backend."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which('cmake'), 'CMake required')
class CrashReportingConfig(unittest.TestCase):
    def configure(self, platform='LINUX', reporting=True, database='test_db'):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / 'Prebuilt.cmake').write_text('''
function(use_prebuilt_binary package)
  file(APPEND "${CMAKE_BINARY_DIR}/packages.log" "${package}\\n")
endfunction()
''')
        (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(CrashReportingConfig NONE)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
# These are synthetic SDK archives: find them consistently on every CI host,
# including when the Windows runner exercises the Darwin selection branch.
set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(LIBS_PREBUILT_DIR "${CMAKE_BINARY_DIR}/packages")
set(ARCH_PREBUILT_DIRS_RELEASE "${LIBS_PREBUILT_DIR}/lib/release")
file(MAKE_DIRECTORY "${ARCH_PREBUILT_DIRS_RELEASE}" "${LIBS_PREBUILT_DIR}/include/breakpad")
foreach(library BugsplatMac CrashReporter HockeySDK)
  file(WRITE "${ARCH_PREBUILT_DIRS_RELEASE}/lib${library}.a" "")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/packages.log" "")
include("''' + (ROOT / 'indra/cmake/bugsplat.cmake').as_posix() + '''")
# Multiple callers must not reset the selected backend or erase its database.
include("''' + (ROOT / 'indra/cmake/bugsplat.cmake').as_posix() + '''")
get_target_property(links ll::bugsplat INTERFACE_LINK_LIBRARIES)
get_target_property(includes ll::bugsplat INTERFACE_INCLUDE_DIRECTORIES)
get_directory_property(directory_definitions COMPILE_DEFINITIONS)
file(WRITE "${CMAKE_BINARY_DIR}/result.txt"
  "enabled=${USE_BUGSPLAT}\\ndatabase=${BUGSPLAT_DB}\\nlinks=${links}\\nincludes=${includes}\\ndefinitions=${directory_definitions}\\n")
''')
        args = ['cmake', '-S', str(root), '-B', str(root / 'build'),
                '-D' + platform + '=ON', '-DINSTALL_PROPRIETARY=' + ('OFF' if platform == 'LINUX' else 'ON'),
                '-DRELEASE_CRASH_REPORTING=' + ('ON' if reporting else 'OFF'),
                '-DBUGSPLAT_DB=' + database, '-DUSE_BUGSPLAT=' + ('OFF' if reporting else 'ON')]
        result = subprocess.run(args, capture_output=True, text=True)
        values = {}
        if (root / 'build/result.txt').exists():
            values = dict(line.split('=', 1) for line in (root / 'build/result.txt').read_text().splitlines())
        packages = (root / 'build/packages.log').read_text().splitlines()
        return result, values, packages

    def test_linux_installs_breakpad_and_keeps_database(self):
        result, values, packages = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(values['enabled'], 'ON')
        self.assertEqual(values['database'], 'test_db')
        self.assertEqual(packages, ['breakpad'])
        self.assertIn('libbreakpad_client.a', values['links'])
        self.assertIn('/include/breakpad', values['includes'])
        self.assertNotIn('__STDC_FORMAT_MACROS', values['definitions'])

    def test_linux_disabled_reporting_does_not_install_backend(self):
        result, values, packages = self.configure(reporting=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(values['enabled'], 'OFF')
        self.assertEqual(values['database'], '')
        self.assertEqual(packages, [])

    def test_linux_missing_database_fails_during_configuration(self):
        result, _, packages = self.configure(database='')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Linux crash reporting requires BUGSPLAT_DB', result.stderr)
        self.assertEqual(packages, [])

    def test_other_platforms_keep_their_bugsplat_sdk(self):
        for platform in ('WINDOWS', 'DARWIN'):
            with self.subTest(platform=platform):
                result, values, packages = self.configure(platform=platform)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(values['enabled'], 'ON')
                self.assertEqual(values['database'], 'test_db')
                self.assertEqual(packages, ['bugsplat'])
                self.assertNotIn('libbreakpad', values['links'])
