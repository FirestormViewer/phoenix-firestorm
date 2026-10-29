"""Exercise the native reporter against local files and a local HTTP server only."""
import email.parser
import email.policy
import fcntl
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest

DRIVER, LOGGER = sys.argv[1:3]
del sys.argv[1:3]


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.run = self.root / 'dump-test'
        self.run.mkdir()
        self.dump = self.run / 'example.dmp'
        self.dump.write_bytes(b'LOCAL TEST DUMP')
        (self.root / 'settings.xml').write_text('PRIVATE SETTINGS')
        (self.root / 'viewer.log').write_text('PRIVATE VIEWER LOG')
        self.context = {'product': 'Firestorm-Releasex64', 'version': '1.2.3.4',
                        'name': 'Test Resident', 'description': 'Test crash',
                        'settings_file': str(self.root / 'settings.xml'),
                        'log_file': str(self.root / 'viewer.log'),
                        'system': {'OS': 'Test Linux'}}
        self.write_context()
        self.requests = []
        outer = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                outer.requests.append((self.headers['Content-Type'], self.rfile.read(int(self.headers['Content-Length']))))
                self.send_response(outer.response)
                self.end_headers()
                self.wfile.write(b'local test response')

            def log_message(self, *args):
                pass

        self.response = 200
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)
        self.url = 'http://127.0.0.1:%s/test' % self.server.server_port

    def stop_server(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def write_context(self):
        (self.run / 'report-context.json').write_text(json.dumps(self.context))

    def native(self, *args, executable=DRIVER, **kwargs):
        return subprocess.run([executable, *map(str, args)], capture_output=True, text=True, timeout=15, **kwargs)

    def capture(self):
        result = self.native('capture', self.dump, '1.2.3.4', 'local_test')
        self.assertEqual(result.returncode, 0, result.stderr)
        return Path(result.stdout.strip())

    def upload(self, manifest):
        return self.native('upload', manifest, self.url)

    def parts(self):
        content_type, body = self.requests[-1]
        message = email.parser.BytesParser(policy=email.policy.default).parsebytes(
            ('Content-Type: %s\r\nMIME-Version: 1.0\r\n\r\n' % content_type).encode() + body)
        return {part.get_param('name', header='content-disposition'): part.get_payload(decode=True)
                for part in message.iter_parts()}

    def test_capture_without_submission_and_respect_preferences(self):
        manifest = self.capture()
        data = json.loads(manifest.read_text())
        self.assertEqual(data['status'], 'pending')
        self.assertEqual(data['name'], 'Test Resident')
        self.assertEqual(self.requests, [])
        self.assertFalse((manifest.parent / 'settings.xml').exists())
        self.assertFalse((manifest.parent / 'Firestorm.log').exists())
        context = json.loads((manifest.parent / 'CrashContext.json').read_text())
        self.assertNotIn('settings_file', context)
        self.assertNotIn('name', context)
        self.assertEqual(manifest.stat().st_mode & 0o777, 0o600)
        self.assertEqual(manifest.parent.stat().st_mode & 0o777, 0o700)

    def test_opted_in_settings_are_snapshotted_not_read_from_later_session(self):
        self.context['include_settings'] = True
        self.write_context()
        manifest = self.capture()
        (self.root / 'settings.xml').write_text('NEXT SESSION SETTINGS')
        self.assertEqual(self.upload(manifest).returncode, 0)
        self.assertEqual(self.parts()['settings.xml'], b'PRIVATE SETTINGS')

    def test_multipart_fields_and_success_prevent_duplicate_upload(self):
        manifest = self.capture()
        result = self.upload(manifest)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        parts = self.parts()
        self.assertEqual(parts['upload_file_minidump'], b'LOCAL TEST DUMP')
        self.assertEqual(parts['product'], b'Firestorm-Releasex64')
        self.assertEqual(parts['version'], b'1.2.3.4')
        self.assertEqual(parts['user'], b'Test Resident')
        self.assertEqual(parts['list_annotations'], b'Test crash')
        self.assertNotIn('settings.xml', parts)
        self.assertNotIn('Firestorm.log', parts)
        self.assertEqual(json.loads(manifest.read_text())['status'], 'sent')
        self.assertIn('Upload accepted', (manifest.parent / 'bugsplat.log').read_text())
        self.assertNotEqual(self.upload(manifest).returncode, 0)
        self.assertEqual(len(self.requests), 1)

    def test_http_failure_retains_report_for_retry(self):
        manifest = self.capture()
        self.response = 503
        self.assertNotEqual(self.upload(manifest).returncode, 0)
        self.assertEqual(json.loads(manifest.read_text())['status'], 'pending')
        self.assertIn('HTTP 503', (manifest.parent / 'bugsplat.log').read_text())
        self.response = 200
        self.assertEqual(self.upload(manifest).returncode, 0)

    def test_invalid_attachment_never_sends_any_request(self):
        manifest = self.capture()
        data = json.loads(manifest.read_text())
        data['attachments'][0]['file'] = '../settings.xml'
        manifest.write_text(json.dumps(data))
        self.assertNotEqual(self.upload(manifest).returncode, 0)
        self.assertEqual(self.requests, [])

    def test_upload_lock_blocks_concurrent_submission(self):
        manifest = self.capture()
        with (manifest.parent / 'upload.lock').open('w') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertNotEqual(self.upload(manifest).returncode, 0)
            self.assertEqual(self.requests, [])

    def test_missing_display_does_not_send_and_can_be_reopened(self):
        env = dict(os.environ, DISPLAY='', WAYLAND_DISPLAY='', SDL_VIDEODRIVER='invalid-test-display')
        result = self.native(self.dump, '1.2.3.4', 'local_test', 'ask', executable=LOGGER, env=env)
        self.assertEqual(result.returncode, 2, result.stderr)
        manifest = self.run / 'report-example' / 'report.json'
        self.assertEqual(json.loads(manifest.read_text())['status'], 'pending')
        self.assertEqual(self.requests, [])
        self.assertIn(str(manifest), self.native('pending', self.root).stdout)
        self.assertEqual(self.native('--report', manifest, executable=LOGGER, env=env).returncode, 2)

    def test_discarded_reports_are_not_retried(self):
        manifest = self.capture()
        data = json.loads(manifest.read_text())
        data['status'] = 'discarded'
        manifest.write_text(json.dumps(data))
        self.assertEqual(self.native('pending', self.root).stdout.strip(), '')
        self.assertNotEqual(self.upload(manifest).returncode, 0)
        self.assertEqual(self.requests, [])

    def test_corrupt_context_still_preserves_dump(self):
        (self.run / 'report-context.json').write_text('{ broken json')
        manifest = self.capture()
        self.assertEqual((manifest.parent / 'minidump.dmp').read_bytes(), b'LOCAL TEST DUMP')

    def test_bad_arguments_do_not_crash(self):
        result = self.native(self.dump, '1.2.3.4', 'local_test', executable=LOGGER)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Usage:', result.stderr)


if __name__ == '__main__':
    unittest.main()
