"""Check ELF selection and failure handling in Linux Breakpad symbol packaging."""

import importlib.util
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('linux_symbol_manifest', ROOT / 'indra/newview/fs_viewer_manifest.py')
manifest_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest_module)


class LinuxSymbols(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        previous = Path.cwd()
        os.chdir(self.root)
        self.addCleanup(os.chdir, previous)
        self.build = self.root / 'build/newview'
        self.build.mkdir(parents=True)
        self.tool = self.root / 'build/packages/bin/dump_syms'
        self.tool.parent.mkdir(parents=True)
        self.tool.write_text('fixture')
        self.manifest = manifest_module.FSViewerManifest()
        self.manifest.args = {'build': str(self.build)}

    def binary(self, name='linux-crash-logger.bin'):
        path = self.root / name
        path.write_bytes(b'\x7fELFfixture')
        return path

    def dump(self, command, *, stdout, **kwargs):
        stdout.write('MODULE Linux x86_64 ABCDEF ' + Path(command[1]).name + '\n'
                     'FILE 0 source.cpp\nFUNC 100 10 0 example\n')
        return subprocess.CompletedProcess(command, 0, stderr='')

    def test_data_files_and_directories_are_not_sent_to_dump_syms(self):
        directory = self.root / 'llplugin'
        directory.mkdir()
        inputs = [directory]
        for name in ('build_data.json', 'ca-bundle.crt', 'v8_context_snapshot.bin',
                     'vk_swiftshader_icd.json'):
            path = self.root / name
            path.write_bytes(b'not an ELF binary')
            inputs.append(path)
        with patch.object(manifest_module.subprocess, 'run') as run:
            for path in inputs:
                self.manifest.fs_generate_breakpad_symbols_for_file(str(path))
        run.assert_not_called()
        self.assertFalse((self.root / 'symbols').exists())

    def test_elf_binary_with_bin_suffix_produces_breakpad_layout(self):
        binary = self.binary()
        with patch.object(manifest_module.subprocess, 'run', side_effect=self.dump) as run:
            self.manifest.fs_generate_breakpad_symbols_for_file(str(binary))
        self.assertEqual(Path(run.call_args.args[0][0]).resolve(), self.tool)
        symbols = self.root / 'symbols' / binary.name / 'ABCDEF' / (binary.name + '.sym')
        self.assertIn('FILE 0 source.cpp', symbols.read_text())
        self.assertIn('FUNC 100 10 0 example', symbols.read_text())
        self.assertFalse(Path(str(binary) + '.sym').exists())

    def test_module_filename_with_spaces_is_preserved(self):
        binary = self.binary('viewer with spaces')
        with patch.object(manifest_module.subprocess, 'run', side_effect=self.dump):
            self.manifest.fs_generate_breakpad_symbols_for_file(str(binary))
        self.assertTrue((self.root / 'symbols' / binary.name / 'ABCDEF' /
                         (binary.name + '.sym')).is_file())

    def test_real_binary_failure_is_fatal_and_names_the_input(self):
        binary = self.binary()
        failure = subprocess.CompletedProcess([], 3, stderr='Failed to write symbol file.')
        with patch.object(manifest_module.subprocess, 'run', return_value=failure):
            with self.assertRaises(RuntimeError) as raised:
                self.manifest.fs_generate_breakpad_symbols_for_file(str(binary))
        self.assertIn(str(binary), str(raised.exception))
        self.assertIn('exit 3', str(raised.exception))
        self.assertFalse(Path(str(binary) + '.sym').exists())
        self.assertFalse((self.root / 'symbols').exists())

    def test_empty_or_invalid_symbol_output_is_rejected(self):
        binary = self.binary()
        for text in ('', 'not a module record\n', 'MODULE Linux x86_64\n'):
            with self.subTest(output=text):
                def dump(command, *, stdout, **kwargs):
                    stdout.write(text)
                    return subprocess.CompletedProcess(command, 0, stderr='')
                with patch.object(manifest_module.subprocess, 'run', side_effect=dump):
                    with self.assertRaisesRegex(RuntimeError, 'no valid MODULE record'):
                        self.manifest.fs_generate_breakpad_symbols_for_file(str(binary))
                self.assertFalse(Path(str(binary) + '.sym').exists())

    def test_symbol_archive_is_complete_when_packaging_returns(self):
        stage = self.build / 'packaged'
        (stage / 'bin').mkdir(parents=True)
        (stage / 'lib').mkdir()
        (stage / 'bin/viewer').write_bytes(b'\x7fELFfixture')
        (stage / 'build_data.json').write_text('{}')
        self.manifest.args.update(dest=str(stage), arch='x86_64',
                                  version=['1', '2', '3', '4'], viewer_flavor='hvk')
        self.manifest.app_name = lambda: 'FirestormPrivate'
        self.manifest.address_size = 64
        with patch.object(manifest_module.subprocess, 'run', side_effect=self.dump):
            self.manifest.fs_save_breakpad_symbols('linux')
        archives = list(self.root.glob('*.tar.bz2'))
        self.assertEqual(len(archives), 1)
        with tarfile.open(archives[0]) as archive:
            self.assertIn('build_data.json', archive.getnames())
            self.assertIn('./viewer/ABCDEF/viewer.sym', archive.getnames())


if __name__ == '__main__':
    unittest.main()
