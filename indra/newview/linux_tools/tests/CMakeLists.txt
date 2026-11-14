# $LicenseInfo:firstyear=2026&license=viewerlgpl$
# Phoenix Firestorm Viewer Source Code
# Copyright (c) 2026, The Phoenix Firestorm Project, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License only.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
# The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
# http://www.firestormviewer.org
# $/LicenseInfo$

include(Python)
find_program(LINUX_WRAPPER_BASH bash)
if (NOT LINUX_WRAPPER_BASH)
    message(FATAL_ERROR "Linux wrapper tests require Bash")
endif ()

# Script tests need no build artifacts. The normal CI CTest invocation runs them
# alongside the binaries built by BUILD_TESTS, independently of the SDL backend.
add_test(NAME linux_wrapper_syntax
    COMMAND "${LINUX_WRAPPER_BASH}" -n "${CMAKE_CURRENT_SOURCE_DIR}/../wrapper.sh")
add_test(NAME linux_wrapper_graphics_policy
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_wrapper.py"
        --bash "${LINUX_WRAPPER_BASH}" --wrapper "${CMAKE_CURRENT_SOURCE_DIR}/../wrapper.sh")
set_tests_properties(linux_wrapper_syntax linux_wrapper_graphics_policy
    PROPERTIES TIMEOUT 60 LABELS "unit;linux;wrapper")
