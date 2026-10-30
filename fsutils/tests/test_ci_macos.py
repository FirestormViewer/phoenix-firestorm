"""Exercise runner cleanup with a fake Xcode layout, never the host's tools."""

import importlib.util
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location('ci_macos', Path(__file__).resolve().parents[1] / 'ci_macos.py')
ci_macos = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci_macos)


class MacDiskCleanup(unittest.TestCase):
    def test_cleanup_cannot_run_locally_or_on_self_hosted_runners(self):
        for system, github, runner in (('Linux', 'true', 'github-hosted'),
                                       ('Darwin', 'false', 'github-hosted'),
                                       ('Darwin', 'true', 'self-hosted')):
            with self.subTest(system=system, github=github, runner=runner), \
                    patch.object(ci_macos.platform, 'system', return_value=system), \
                    patch.dict(os.environ, {'GITHUB_ACTIONS': github, 'RUNNER_ENVIRONMENT': runner}), \
                    patch.object(sys, 'argv', ['ci_macos.py', 'prepare']), \
                    patch.object(ci_macos, 'prepare') as prepare:
                with self.assertRaises(SystemExit):
                    ci_macos.main()
                prepare.assert_not_called()

    def test_selected_xcode_aliases_and_macos_sdk_survive_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            apps = Path(directory) / 'Applications'
            developer = apps / 'Xcode_16.4.app/Contents/Developer'
            sdk = developer / 'Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk'
            sdk.mkdir(parents=True)
            (apps / 'Xcode.app').symlink_to(apps / 'Xcode_16.4.app', target_is_directory=True)
            (apps / 'Xcode_16.4.0.app').symlink_to(apps / 'Xcode_16.4.app', target_is_directory=True)
            unused = apps / 'Xcode_26.3.app'
            unused.mkdir()
            (apps / 'Xcode_26.3.0.app').symlink_to(unused, target_is_directory=True)
            for name in ci_macos.MOBILE_PLATFORMS:
                (developer / 'Platforms' / (name + '.platform')).mkdir()
            actual, paths = ci_macos.cleanup_paths(apps / 'Xcode.app/Contents/Developer', apps)
            self.assertEqual(actual, developer)
            for path in paths:
                if path.is_symlink():
                    path.unlink()
                elif path.exists():
                    shutil.rmtree(path)
            self.assertTrue(sdk.is_dir())
            self.assertTrue((apps / 'Xcode.app/Contents/Developer').is_dir())
            self.assertTrue((apps / 'Xcode_16.4.0.app').is_dir())
            self.assertFalse(unused.exists())
            self.assertFalse(any((developer / 'Platforms' / (name + '.platform')).exists()
                                 for name in ci_macos.MOBILE_PLATFORMS))

    def test_invalid_selected_directory_is_rejected_before_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            apps = Path(directory)
            (apps / 'CommandLineTools').mkdir()
            with self.assertRaises(ValueError):
                ci_macos.cleanup_paths(apps / 'CommandLineTools', apps)

    def test_only_download_cache_is_removed_after_configure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / 'firestorm-autobuild-cache'
            cache.mkdir()
            (cache / 'dependency.tar.zst').write_text('archive')
            packages = root / 'build-darwin-universal/packages'
            packages.mkdir(parents=True)
            (packages / 'installed.xml').write_text('installed dependency')
            with patch.dict(os.environ, {'RUNNER_TEMP': str(root), 'AUTOBUILD_INSTALLABLE_CACHE': str(cache)}), \
                    patch.object(ci_macos, 'report'), patch.object(ci_macos.subprocess, 'run'):
                ci_macos.after_configure()
                self.assertFalse(cache.exists())
                self.assertTrue((packages / 'installed.xml').is_file())
                for unexpected in (root, packages):
                    with patch.dict(os.environ, {'AUTOBUILD_INSTALLABLE_CACHE': str(unexpected)}), \
                            self.assertRaises(ValueError):
                        ci_macos.after_configure()
                cache.symlink_to(packages, target_is_directory=True)
                with self.assertRaises(ValueError):
                    ci_macos.after_configure()
                self.assertTrue(packages.is_dir())


if __name__ == '__main__':
    unittest.main()
