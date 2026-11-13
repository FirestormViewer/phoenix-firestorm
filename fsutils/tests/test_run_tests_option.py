"""Run the build wrapper against a tiny CMake project, without viewer dependencies."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == 'posix' and all(shutil.which(tool) for tool in
                                             ('bash', 'cmake', 'ctest', 'make')),
                     'POSIX shell and CMake/CTest/Make required')
class RunTestsOption(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.build = root / 'build'
        self.build.mkdir()
        source = root / 'indra'
        (source / 'newview').mkdir(parents=True)
        (source / 'newview/VIEWER_VERSION_FS.txt').write_text('7.3.0\n')
        (root / '.git').mkdir()
        tools = root / 'tools'
        tools.mkdir()
        git = tools / 'git'
        git.write_text('#!/bin/sh\nprintf "fixture\\n"\n')
        git.chmod(0o755)
        (root / 'variables').write_text('')
        self.env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'],
                        AUTOBUILD_VARIABLES_FILE=str(root / 'variables'), AUTOBUILD_ADDRSIZE='64')
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(RunTestsOption NONE)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/newview")
file(WRITE "${CMAKE_BINARY_DIR}/newview/viewer_version.txt" "7.3.0.1")
option(LL_TESTS "Enable tests" OFF)
option(FIXTURE_FAIL_TEST "Fail the test" OFF)
option(FIXTURE_FAIL_BUILD "Fail test compilation" OFF)
option(FIXTURE_NO_TESTS "Register no tests" OFF)
add_custom_target(viewer ALL COMMAND "${CMAKE_COMMAND}" -E touch viewer-built)
if(LL_TESTS)
  if(FIXTURE_FAIL_BUILD)
    add_custom_target(BUILD_TESTS COMMAND "${CMAKE_COMMAND}" -E false)
  else()
    add_custom_target(BUILD_TESTS COMMAND "${CMAKE_COMMAND}" -E touch tests-built)
  endif()
  enable_testing()
  if(NOT FIXTURE_NO_TESTS)
    add_test(NAME wrapper_fixture COMMAND "${CMAKE_COMMAND}"
      -DFAIL_TEST=${FIXTURE_FAIL_TEST} -P "${CMAKE_CURRENT_SOURCE_DIR}/fixture.cmake")
  endif()
endif()
''')
        (source / 'fixture.cmake').write_text('''
if(NOT EXISTS viewer-built OR NOT EXISTS tests-built)
  message(FATAL_ERROR "Viewer and tests must be built before running tests")
endif()
file(WRITE tests-ran "ran")
if(FAIL_TEST)
  message(FATAL_ERROR "Intentional fixture failure")
endif()
''')

    def run_script(self, *args):
        return subprocess.run(['bash', str(ROOT / 'scripts/configure_firestorm.sh'),
                               '--platform', 'linux', '--jobs', '2', *args],
                              cwd=self.build, env=self.env, text=True, capture_output=True,
                              timeout=30)

    def assert_succeeded(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_flag_builds_and_runs_registered_tests(self):
        self.assert_succeeded(self.run_script('--config', '--build', '--run-tests'))
        self.assertTrue((self.build / 'tests-ran').is_file())

    def test_configure_only_enables_tests_without_running_them(self):
        self.assert_succeeded(self.run_script('--config', '--run-tests'))
        self.assertIn('LL_TESTS:BOOL=ON', (self.build / 'CMakeCache.txt').read_text())
        self.assertFalse((self.build / 'viewer-built').exists())
        self.assertFalse((self.build / 'tests-ran').exists())
        self.assert_succeeded(self.run_script('--build', '--run-tests'))
        self.assertTrue((self.build / 'tests-ran').is_file())

    def test_normal_build_keeps_tests_disabled(self):
        self.assert_succeeded(self.run_script('--config', '--build'))
        self.assertIn('LL_TESTS:BOOL=OFF', (self.build / 'CMakeCache.txt').read_text())
        self.assertTrue((self.build / 'viewer-built').is_file())
        self.assertFalse((self.build / 'tests-ran').exists())

    def test_disabled_tests_are_rejected_before_building(self):
        self.assert_succeeded(self.run_script('--config'))
        result = self.run_script('--build', '--run-tests')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Reconfigure with --run-tests', result.stdout)
        self.assertFalse((self.build / 'viewer-built').exists())

    def test_test_failure_is_not_hidden_by_log_pipeline(self):
        result = self.run_script('--config', '--build', '--run-tests', '-DFIXTURE_FAIL_TEST=ON')
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.build / 'tests-ran').is_file())
        self.assertIn('Intentional fixture failure', result.stdout)
        self.assertIn('Tests failed!', result.stdout)

    def test_empty_suite_is_a_failure(self):
        result = self.run_script('--config', '--build', '--run-tests', '-DFIXTURE_NO_TESTS=ON')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('No tests were found', result.stdout)

    def test_test_compilation_failure_prevents_running_tests(self):
        result = self.run_script('--config', '--build', '--run-tests', '-DFIXTURE_FAIL_BUILD=ON')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Test compilation failed!', result.stdout)
        self.assertFalse((self.build / 'tests-ran').exists())

    @unittest.skipUnless(shutil.which('ninja'), 'Ninja required')
    def test_ninja_build_runs_tests(self):
        self.assert_succeeded(self.run_script('--config', '--build', '--ninja', '--run-tests'))
        self.assertTrue((self.build / 'tests-ran').is_file())


if __name__ == '__main__':
    unittest.main()
