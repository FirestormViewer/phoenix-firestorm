"""Check macOS installer assets without mounting images or running Apple tools."""

from contextlib import redirect_stdout
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2] / 'indra/newview'
spec = importlib.util.spec_from_file_location('dmg_manifest', SOURCE / 'fs_viewer_manifest.py')
manifest_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest_module)


class DmgTemplates(unittest.TestCase):
    def manifest(self, source, channel, flavor='hvk'):
        manifest = manifest_module.FSViewerManifest()
        manifest.args = {'viewer_flavor': flavor}
        manifest.channel_type = lambda: channel
        manifest.src_path_of = lambda path: str(source / path)
        return manifest

    def select(self, manifest):
        with redirect_stdout(io.StringIO()):
            return Path(manifest.fs_mac_dmg_template())

    def test_shipped_channels_use_their_own_artwork_for_each_flavor(self):
        for flavor, prefix in (('hvk', 'firestorm'), ('oss', 'firestormos')):
            for channel in ('release', 'beta', 'nightly', 'private'):
                with self.subTest(flavor=flavor, channel=channel):
                    selected = self.select(self.manifest(SOURCE, channel, flavor))
                    self.assertEqual(selected, Path('installers/darwin') / f'{prefix}-{channel}-dmg')

    def test_channels_without_artwork_use_same_flavor_release_assets(self):
        for flavor, prefix in (('hvk', 'firestorm'), ('oss', 'firestormos')):
            for channel in ('alpha', 'manual', 'profiling'):
                with self.subTest(flavor=flavor, channel=channel):
                    selected = self.select(self.manifest(SOURCE, channel, flavor))
                    self.assertEqual(selected, Path('installers/darwin') / f'{prefix}-release-dmg')

    def test_incomplete_template_reports_missing_asset(self):
        names = ('background.png', '_DS_Store', '_VolumeIcon.icns', 'Applications-alias.r')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            template = source / 'installers/darwin/firestorm-release-dmg'
            template.mkdir(parents=True)
            for name in names:
                (template / name).write_bytes(b'fixture')
            for name in names:
                with self.subTest(asset=name):
                    asset = template / name
                    asset.unlink()
                    with self.assertRaises(FileNotFoundError) as raised:
                        self.select(self.manifest(source, 'manual'))
                    self.assertIn(str(asset), str(raised.exception))
                    asset.write_bytes(b'fixture')

    def test_missing_fallback_reports_all_required_assets_for_correct_flavor(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            with self.assertRaises(FileNotFoundError) as raised:
                self.select(self.manifest(source, 'profiling', 'oss'))
            template = source / 'installers/darwin/firestormos-release-dmg'
            for name in ('background.png', '_DS_Store', '_VolumeIcon.icns', 'Applications-alias.r'):
                self.assertIn(str(template / name), str(raised.exception))


if __name__ == '__main__':
    unittest.main()
