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

"""Exercise the packaged installer and menu helper in a private user installation."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[1]


class InstallerTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="firestorm-install-")
        self.addCleanup(temp.cleanup)
        self.home = Path(temp.name)
        self.run_as = {}
        if os.geteuid() == 0:
            # Always exercise the user install, even in a root-owned CI container.
            import pwd
            nobody = pwd.getpwnam("nobody")
            os.chown(self.home, nobody.pw_uid, nobody.pw_gid)
            self.run_as = dict(user=nobody.pw_uid, group=nobody.pw_gid, extra_groups=[])

        self.package = self.home / "bundle"
        (self.package / "etc").mkdir(parents=True)
        self.installer = shutil.copy2(TOOLS / "install.sh", self.package)
        shutil.copy2(TOOLS / "refresh_desktop_app_entry.sh", self.package / "etc")
        viewer = self.package / "firestorm"
        viewer.write_text("#!/bin/sh\necho test-viewer-started\n")
        viewer.chmod(0o755)

        self.prefix = self.home / "Firestorm Viewer"
        self.data = self.home / "desktop data"
        self.entry = self.data / "applications/firestorm-viewer.desktop"
        self.env = dict(PATH=os.defpath, HOME=str(self.home), XDG_DATA_HOME=str(self.data), LC_ALL="C")

    def run_command(self, args, answers=""):
        return subprocess.run(args, input=answers, cwd=self.home, env=self.env,
                              text=True, capture_output=True, timeout=10, **self.run_as)

    def install(self, *args, answers=""):
        return self.run_command([self.installer, "--install-dir", str(self.prefix), *args], answers)

    def check_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fresh_and_identical_reinstall(self):
        self.check_success(self.install("--yes"))
        self.assertTrue(self.entry.is_file())
        self.entry.chmod(0o755)
        before = self.entry.stat()
        result = self.install(answers="\ny\n")
        self.check_success(result)
        self.assertNotIn("[y/N]", result.stdout)
        after = self.entry.stat()
        self.assertEqual((after.st_ino, after.st_mode, after.st_mtime_ns),
                         (before.st_ino, before.st_mode, before.st_mtime_ns))

    @unittest.skipUnless(shutil.which("gio"), "gio is required to launch the menu entry")
    def test_launch_from_custom_path(self):
        self.prefix = self.home / 'Firestorm "100%" build'
        self.check_success(self.install("--yes"))
        result = self.run_command(["gio", "launch", str(self.entry)])
        self.check_success(result)
        self.assertIn("test-viewer-started", result.stdout)
        (self.prefix / "firestorm").chmod(0o644)
        result = self.run_command(["gio", "launch", str(self.entry)])
        self.assertNotIn("test-viewer-started", result.stdout)

    def test_overwrite_choices(self):
        cases = (
            (["--yes"], "y\n", False, False),
            ([], "\n\n", True, False),
            ([], "\nn\n", True, False),
            ([], "\ny\n", True, True),
        )
        self.check_success(self.run_command(["mkdir", "-p", str(self.entry.parent)]))
        for i, (args, answers, prompt, replace) in enumerate(cases):
            with self.subTest(args=args, answers=answers):
                # A fresh prefix for each case avoids exercising backup retention.
                self.prefix = self.home / f"install-{i}"
                custom = "[Desktop Entry]\nExec=my-nvidia-launcher\n"
                self.entry.write_text(custom)
                self.entry.chmod(0o755)
                result = self.install(*args, answers=answers)
                self.check_success(result)
                self.assertTrue((self.prefix / "firestorm").is_file())
                self.assertEqual("[y/N]" in result.stdout, prompt)
                self.assertEqual(self.entry.stat().st_mode & 0o777, 0o755)
                if replace:
                    self.assertIn(str(self.prefix / "firestorm"), self.entry.read_text())
                else:
                    self.assertEqual(self.entry.read_text(), custom)

    def test_failed_registration_is_reported_after_copy(self):
        tools = self.home / "commands"
        tools.mkdir()
        failed_install = tools / "desktop-file-install"
        failed_install.write_text("#!/bin/sh\nexit 1\n")
        failed_install.chmod(0o755)
        self.env["PATH"] = str(tools) + os.pathsep + os.defpath
        result = self.install("--yes")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.prefix / "firestorm").is_file())
        self.assertFalse(self.entry.exists())
        self.assertIn("desktop menu registration failed", result.stderr)

        # Keeping a user-managed link needs no registration tools, even when
        # its target (e.g. on a removable drive) is currently unavailable.
        self.check_success(self.run_command(["mkdir", "-p", str(self.entry.parent)]))
        self.entry.symlink_to(self.home / "my-launcher.desktop")
        self.check_success(self.install("--yes", answers="y\n"))
        self.assertEqual(Path(os.readlink(self.entry)), self.home / "my-launcher.desktop")

    def test_entry_created_during_registration(self):
        # Create a customised entry after the initial check, before publishing.
        # Driving that point via the staging tool avoids a timing-dependent test.
        commands = self.home / "commands"
        commands.mkdir()
        tool = commands / "desktop-file-install"
        tool.write_text(
            '#!/bin/sh\n'
            'echo "Exec=my-launcher" > "$XDG_DATA_HOME/applications/firestorm-viewer.desktop"\n'
            'PATH="$SYSTEM_PATH" exec desktop-file-install "$@"\n'
        )
        tool.chmod(0o755)
        self.env["SYSTEM_PATH"] = self.env["PATH"]
        self.env["PATH"] = str(commands) + os.pathsep + self.env["PATH"]
        self.check_success(self.install("--yes"))
        self.assertEqual(self.entry.read_text(), "Exec=my-launcher\n")

    def test_default_data_directory(self):
        del self.env["XDG_DATA_HOME"]
        self.check_success(self.install("--yes"))
        self.assertTrue((self.home / ".local/share/applications/firestorm-viewer.desktop").is_file())


if __name__ == "__main__":
    if not all(shutil.which(tool) for tool in ("desktop-file-install", "update-desktop-database")):
        print("Linux installer tests require desktop-file-utils")
        sys.exit(77)
    unittest.main(verbosity=2)
