#!/usr/bin/env python3
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

"""Run the production launcher with fake PCI sysfs and a recording viewer."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest


WRAPPER = Path(__file__).resolve().parents[1] / "wrapper.sh"
BASH = "/bin/bash"
NVIDIA = ("0x10de", "0x030000", "nvidia")
INTEL = ("0x8086", "0x030000", "i915")


class WrapperTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="firestorm-wrapper-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.devices = self.root / "devices"
        self.devices.mkdir()
        source = WRAPPER.read_text()
        # Only redirect the hardware boundary. Execute all production decisions
        # and the real launch path; never read the test machine's PCI devices.
        pci_glob = "/sys/bus/pci/devices/*"
        self.assertEqual(source.count(pci_glob), 1, "Update the sysfs fixture boundary")
        self.launcher = self.root / "firestorm"
        self.launcher.write_text(source.replace(pci_glob, shlex.quote(str(self.devices)) + "/*"))
        (self.root / "etc").mkdir()
        for name in ("register_secondlifeprotocol.sh", "refresh_desktop_app_entry.sh"):
            stub = self.root / "etc" / name
            stub.write_text("#!/bin/sh\nexit 0\n")
            stub.chmod(0o755)
        (self.root / "bin").mkdir()
        viewer = self.root / "bin" / "do-not-directly-run-firestorm-bin"
        viewer.write_text(
            "#!/bin/sh\nenv -0 > viewer-env\n"
            "printf '%s\\0' \"$@\" > viewer-args\n"
        )
        viewer.chmod(0o755)

    def add_device(self, vendor, device_class, driver):
        device = self.devices / str(len(list(self.devices.iterdir())))
        device.mkdir()
        for name, value in (("vendor", vendor), ("class", device_class)):
            if value is not None:
                (device / name).write_text(value + "\n")
        if driver is not None:
            driver_dir = self.root / "drivers" / driver
            driver_dir.mkdir(parents=True, exist_ok=True)
            (device / "driver").symlink_to(driver_dir, target_is_directory=True)

    def launch(self, settings):
        # A clean environment makes the result independent of the runner's GPU,
        # desktop, shell startup files and any inherited loader/SDL overrides.
        env = {"PATH": os.defpath, "HOME": str(self.root), "LC_ALL": "C"}
        env.update(settings)
        output = self.root / "viewer-env"
        output.unlink(missing_ok=True)
        result = subprocess.run(
            [BASH, str(self.launcher), "--skip-gridargs", "--test-argument", "two words"],
            cwd=self.root, env=env, capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(output.exists(), result.stdout + result.stderr)
        captured = dict(entry.split("=", 1) for entry in output.read_text().split("\0")[:-1])
        arguments = (self.root / "viewer-args").read_text().split("\0")[:-1]
        self.assertEqual(arguments, ["--test-argument", "two words"])
        self.assertEqual(captured.get("mesa_glthread"), "true")
        return captured

    def assert_policy(self, settings, expected_sdl, expected_threading):
        actual = self.launch(settings)
        for name in ("SDL_VIDEO_DRIVER", "SDL_VIDEODRIVER"):
            self.assertEqual(actual.get(name), expected_sdl.get(name), name)
        self.assertEqual(actual.get("__GL_THREADED_OPTIMIZATIONS"), expected_threading)
        for name in ("WAYLAND_DISPLAY", "XDG_SESSION_TYPE", "DISPLAY"):
            self.assertEqual(actual.get(name), settings.get(name), name)

    def test_hardware_detection(self):
        cases = [
            ("no devices", [], False),
            ("NVIDIA display", [NVIDIA], True),
            ("NVIDIA 3D controller", [("0x10de", "0x030200", "nvidia")], True),
            ("NVIDIA other display", [("0x10de", "0x038000", "nvidia")], True),
            ("Intel display", [INTEL], False),
            ("AMD display", [("0x1002", "0x030000", "amdgpu")], False),
            ("Nouveau", [("0x10de", "0x030000", "nouveau")], False),
            ("unbound NVIDIA", [("0x10de", "0x030000", None)], False),
            ("NVIDIA audio", [("0x10de", "0x040300", "nvidia")], False),
            ("missing vendor", [(None, "0x030000", "nvidia")], False),
            ("missing class", [("0x10de", None, "nvidia")], False),
            ("hybrid", [INTEL, NVIDIA], True),
            ("NVIDIA before Intel", [NVIDIA, INTEL], True),
        ]
        for name, devices, detected in cases:
            with self.subTest(hardware=name):
                # Separate fixture namespaces keep every topology independent.
                for device in self.devices.iterdir():
                    for entry in device.iterdir():
                        entry.unlink()
                    device.rmdir()
                for device in devices:
                    self.add_device(*device)
                settings = {"WAYLAND_DISPLAY": "wayland-0", "__GL_THREADED_OPTIMIZATIONS": "keep"}
                expected = {"SDL_VIDEO_DRIVER": "x11", "SDL_VIDEODRIVER": "x11"} if detected else {}
                self.assert_policy(settings, expected, "1" if detected else "keep")

    def check_sessions_and_overrides(self, nvidia):
        self.add_device(*(NVIDIA if nvidia else INTEL))
        sessions = [
            ({}, False),
            ({"XDG_SESSION_TYPE": "x11"}, False),
            ({"DISPLAY": ":1"}, False),
            ({"XDG_SESSION_TYPE": "wayland"}, True),
            ({"WAYLAND_DISPLAY": "wayland-0"}, True),
            ({"XDG_SESSION_TYPE": "x11", "WAYLAND_DISPLAY": "wayland-0"}, True),
            ({"XDG_SESSION_TYPE": "wayland", "DISPLAY": ":1"}, True),
            ({"XDG_SESSION_TYPE": "", "WAYLAND_DISPLAY": ""}, False),
        ]
        # Expected backend is explicit test data, including modern-name
        # precedence and empty-value fallback to the legacy SDL variable.
        overrides = [
            ({}, None),
            ({"SDL_VIDEO_DRIVER": "", "SDL_VIDEODRIVER": ""}, None),
            ({"SDL_VIDEO_DRIVER": "x11"}, "x11"),
            ({"SDL_VIDEODRIVER": "x11"}, "x11"),
            ({"SDL_VIDEO_DRIVER": "wayland"}, "wayland"),
            ({"SDL_VIDEODRIVER": "wayland"}, "wayland"),
            ({"SDL_VIDEO_DRIVER": "dummy"}, "dummy"),
            ({"SDL_VIDEO_DRIVER": "x11,wayland"}, "x11,wayland"),
            ({"SDL_VIDEO_DRIVER": "wayland,x11"}, "wayland,x11"),
            ({"SDL_VIDEO_DRIVER": "x11", "SDL_VIDEODRIVER": "wayland"}, "x11"),
            ({"SDL_VIDEO_DRIVER": "wayland", "SDL_VIDEODRIVER": "x11"}, "wayland"),
            ({"SDL_VIDEO_DRIVER": "", "SDL_VIDEODRIVER": "x11"}, "x11"),
        ]
        for session, wayland in sessions:
            for override, backend in overrides:
                for previous in (None, "0", "1"):
                    with self.subTest(session=session, override=override, previous=previous):
                        settings = dict(session, **override)
                        if previous is not None:
                            settings["__GL_THREADED_OPTIMIZATIONS"] = previous
                        expected = dict(override)
                        threading = previous
                        if nvidia:
                            if wayland and backend is None:
                                expected = {"SDL_VIDEO_DRIVER": "x11", "SDL_VIDEODRIVER": "x11"}
                            threading = "1" if backend in (None, "x11") else "0"
                        self.assert_policy(settings, expected, threading)

    def test_nvidia_sessions_and_overrides(self):
        self.check_sessions_and_overrides(True)

    def test_other_gpu_settings_are_untouched(self):
        self.check_sessions_and_overrides(False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wrapper", type=Path, default=WRAPPER)
    parser.add_argument("--bash", default=BASH)
    options, remaining = parser.parse_known_args()
    WRAPPER = options.wrapper.resolve()
    BASH = options.bash
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
