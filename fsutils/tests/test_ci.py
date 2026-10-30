"""Regression checks for the CI deployment boundary and result reporting."""
import argparse
from contextlib import redirect_stdout
import io
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'fsutils'))
import ci

spec = importlib.util.spec_from_file_location('fs_viewer_manifest', ROOT / 'indra/newview/fs_viewer_manifest.py')
manifest_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest_module)


class Workspace(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        old = Path.cwd()
        self.addCleanup(os.chdir, old)
        os.chdir(self.tmp.name)
        self.env = patch.dict(os.environ, {
            'PATH': os.environ.get('PATH', ''), 'GITHUB_REPOSITORY': 'example/viewer',
            'GITHUB_SHA': 'abc123', 'GITHUB_RUN_ID': '42', 'GITHUB_RUN_ATTEMPT': '2',
            'GITHUB_OUTPUT': str(Path('outputs').resolve()),
            'GITHUB_STEP_SUMMARY': str(Path('summary').resolve()),
        }, clear=True)
        self.env.start()
        self.addCleanup(self.env.stop)

    def fixture(self, cells=None):
        cells = ci.matrix() if cells is None else cells
        plan = dict(ci.identity(), release_type='Nightly', version='7.3.0', build='123', cells=cells)
        records, jobs, artifacts = [], [], []
        for index, cell in enumerate(cells, start=1):
            records.append(dict(ci.identity(), cell=cell,
                                steps=dict.fromkeys(ci.REQUIRED_STEPS, 'success'), artifact_id=str(index),
                                tests={'state': 'passed', 'total': 5, 'passed': 5, 'failed': 0, 'skipped': 0},
                                deliverables=[{'name': 'Phoenix-test.dmg', 'size': 5, 'sha256': 'unused'}]))
            jobs.append({'name': 'Build / ' + ci.cell_key(cell), 'conclusion': 'success',
                         'html_url': 'https://github.com/example/job', 'steps': []})
            artifacts.append({'id': index, 'name': ci.artifact_name('Nightly', cell),
                              'expired': False, 'size_in_bytes': 100})
        return plan, records, jobs, artifacts



class CIWorkflow(Workspace):
    def test_policy_and_matrix(self):
        self.assertEqual(ci.release_type('workflow_dispatch', 'Firestorm7.3.0'), 'Manual')
        self.assertEqual(ci.release_type('workflow_dispatch', 'main', True), 'Profiling')
        self.assertEqual(ci.release_type('schedule', 'Firestorm7.3.0'), 'Nightly')
        for branch, expected in [('Firestorm7.3.0', 'Release'), ('7.3preview', 'Beta'),
                                 ('7.3alpha', 'Alpha'), ('7.3nightly', 'Nightly')]:
            self.assertEqual(ci.release_type('push', branch), expected)
        with self.assertRaises(ValueError):
            ci.release_type('push', 'unknown')
        self.assertEqual(len(ci.matrix()), 12)
        self.assertEqual(len(ci.matrix(True)), 1)
        for channel in ('Manual', 'Profiling'):
            self.assertEqual(ci.CHANNELS[channel], ('test', 'MANUAL_WEBHOOK_URL'))

    def test_complete_matrix_passes(self):
        gate = ci.check_results(*self.fixture(), 'success')
        self.assertTrue(gate['passed'])
        self.assertEqual(len(gate['results']), 12)

    def test_failure_cancellation_timeout_and_missing_job_block(self):
        for conclusion in ('failure', 'cancelled', 'timed_out', None):
            with self.subTest(conclusion=conclusion):
                plan, records, jobs, artifacts = self.fixture()
                jobs[0]['conclusion'] = conclusion
                jobs[0]['steps'] = [{'name': 'Build/package', 'conclusion': 'failure'}]
                gate = ci.check_results(plan, records, jobs, artifacts, 'failure')
                self.assertFalse(gate['passed'])
                self.assertIn('Build/package', ci.report_text(gate))
        plan, records, jobs, artifacts = self.fixture()
        self.assertFalse(ci.check_results(plan, records, jobs[1:], artifacts, 'success')['passed'])

    def test_missing_failed_empty_or_skipped_test_results_block(self):
        for change in ('missing', 'failed', 'empty', 'all-skipped', 'compilation'):
            with self.subTest(change=change):
                plan, records, jobs, artifacts = self.fixture()
                if change == 'missing':
                    records.pop(0)
                elif change == 'failed':
                    records[0]['tests'].update(state='failed', failed=1)
                elif change == 'empty':
                    records[0]['tests'].update(total=0, passed=0)
                elif change == 'all-skipped':
                    records[0]['tests'].update(passed=0, skipped=5)
                else:
                    records[0]['steps']['build'] = 'failure'
                self.assertFalse(ci.check_results(plan, records, jobs, artifacts, 'success')['passed'])

    def test_missing_wrong_or_expired_artifacts_block(self):
        for change in ('missing', 'expired', 'wrong-name', 'empty', 'no-deliverable'):
            with self.subTest(change=change):
                plan, records, jobs, artifacts = self.fixture()
                if change == 'missing':
                    artifacts.pop(0)
                elif change == 'expired':
                    artifacts[0]['expired'] = True
                elif change == 'wrong-name':
                    artifacts[0]['name'] = 'symbols-only'
                elif change == 'empty':
                    artifacts[0]['size_in_bytes'] = 0
                else:
                    records[0]['deliverables'] = []
                self.assertFalse(ci.check_results(plan, records, jobs, artifacts, 'success')['passed'])

    def test_stale_and_duplicate_results_rejected(self):
        for field in ('sha', 'attempt', 'run_id', 'repository'):
            plan, records, jobs, artifacts = self.fixture()
            records[0][field] = 'wrong'
            with self.subTest(field=field), self.assertRaises(ValueError):
                ci.check_results(plan, records, jobs, artifacts, 'success')
        plan, records, jobs, artifacts = self.fixture()
        with self.assertRaises(ValueError):
            ci.check_results(plan, records + records[:1], jobs, artifacts, 'success')
        self.assertFalse(ci.check_results(*self.fixture(), 'failure')['passed'])

    def test_junit_totals_and_failures(self):
        Path('junit.xml').write_text('<testsuite><testcase name="pass"/>'
                                    '<testcase name="fail"><failure/></testcase>'
                                    '<testcase name="skip"><skipped/></testcase></testsuite>')
        self.assertEqual(ci.junit_summary('junit.xml'), {
            'total': 3, 'passed': 1, 'failed': 1, 'skipped': 1, 'failures': ['fail']})
        for xml in ('<testsuite/>', '<testsuite><testcase><skipped/></testcase></testsuite>'):
            Path('junit.xml').write_text(xml)
            with self.assertRaises(ValueError):
                ci.junit_summary('junit.xml')

    def test_discord_report_is_bounded_routed_and_has_no_mentions(self):
        for result in ('success', 'failure'):
            gate = ci.check_results(*self.fixture(), result)
            for channel, (_, secret) in ci.CHANNELS.items():
                with self.subTest(result=result, channel=channel):
                    gate['release_type'] = channel
                    os.environ[secret] = 'https://discord.invalid/test'
                    with patch.object(ci, 'request_json') as send:
                        ci.send_report(gate)
                        self.assertEqual(send.call_args.args[0], os.environ[secret])
                        payload = send.call_args.kwargs['payload']
                        self.assertLessEqual(len(payload['content']), 2000)
                        self.assertEqual(payload['allowed_mentions'], {'parse': []})
                        expected = 'DEPLOYMENT ALLOWED' if result == 'success' else 'DEPLOYMENT BLOCKED'
                        self.assertIn(expected, payload['content'])
                        self.assertIn('/actions/runs/42/attempts/2', payload['content'])
                        if result == 'success':
                            self.assertIn('CI passed', payload['content'])
                            self.assertIn('CTest: 5 passed', payload['content'])

    def test_successful_manual_collect_posts_report_without_deployment(self):
        plan, records, jobs, artifacts = self.fixture()
        plan['release_type'] = 'Manual'
        for artifact, cell in zip(artifacts, plan['cells']):
            artifact['name'] = ci.artifact_name('Manual', cell)
        ci.write_json('ci-plan.json', plan)
        for index, record in enumerate(records):
            ci.write_json(Path('ci-reports') / str(index) / 'result.json', record)
        os.environ.update(CI_MATRIX_RESULT='success', MANUAL_WEBHOOK_URL='https://discord.invalid/test')
        with redirect_stdout(io.StringIO()), patch.object(ci, 'pages', side_effect=[jobs, artifacts]), \
                patch.object(ci, 'request_json') as send:
            ci.collect(None)
        send.assert_called_once()
        self.assertEqual(send.call_args.args[0], os.environ['MANUAL_WEBHOOK_URL'])
        self.assertIn('Firestorm Manual — CI passed', send.call_args.kwargs['payload']['content'])
        self.assertTrue(ci.read_json('ci-gate.json')['passed'])
        self.assertIn('artifact_ids=', Path(os.environ['GITHUB_OUTPUT']).read_text())

    def test_collection_failure_still_notifies_and_blocks(self):
        plan, _, _, _ = self.fixture()
        ci.write_json('ci-plan.json', plan)
        os.environ['CI_MATRIX_RESULT'] = 'failure'
        captured = io.StringIO()
        # The simulated failure emits a real Actions annotation. Keep it out of
        # the surrounding test runner's output while asserting it is generated.
        with redirect_stdout(captured), patch.object(ci, 'pages', side_effect=ValueError('API unavailable')):
            with patch.object(ci, 'send_report') as send:
                with self.assertRaises(ValueError):
                    ci.collect(None)
                self.assertFalse(send.call_args.args[0]['passed'])
        self.assertIn('::error::CI result collection failed: API unavailable', captured.getvalue())
        self.assertIn('DEPLOYMENT BLOCKED', captured.getvalue())
        self.assertFalse(ci.read_json('ci-gate.json')['passed'])

    def test_notification_delivery_retries_are_bounded_and_redacted(self):
        url = 'https://discord.invalid/private-webhook-token'
        for code, attempts in ((429, 3), (500, 3), (401, 1)):
            with self.subTest(code=code):
                error = HTTPError(url, code, 'request failed', {'Retry-After': '0'}, None)
                with patch.object(ci, 'urlopen', side_effect=error) as send:
                    with patch.object(ci.time, 'sleep'):
                        with self.assertRaises(ValueError) as raised:
                            ci.request_json(url, payload={'content': 'CI failed'})
                self.assertEqual(send.call_count, attempts)
                self.assertNotIn('private-webhook-token', str(raised.exception))

    def test_record_does_not_copy_arbitrary_step_outputs(self):
        cell = ci.matrix()[0]
        os.environ['CI_CELL'] = json.dumps(cell)
        os.environ['CI_STEPS'] = json.dumps({
            'build': {'outcome': 'failure', 'outputs': {'unexpected': 'private-value'}},
            'publish_macos': {'outcome': 'skipped', 'outputs': {}}})
        ci.record(None)
        result = Path('ci-results/result.json').read_text()
        self.assertNotIn('private-value', result)
        self.assertEqual(json.loads(result)['steps']['build'], 'failure')

    def test_compatible_webhook_can_acknowledge_with_plain_text(self):
        response = MagicMock()
        response.__enter__.return_value.read.return_value = b'ok'
        with patch.object(ci, 'urlopen', return_value=response):
            self.assertIsNone(ci.request_json('https://discord.invalid/test', payload={'content': 'failed'}))

    def test_report_prioritizes_failure_over_passed_combinations(self):
        plan, records, jobs, artifacts = self.fixture()
        jobs[-1]['conclusion'] = 'failure'
        gate = ci.check_results(plan, records, jobs, artifacts, 'failure')
        text = ci.report_text(gate, failures_only=True)
        self.assertIn(ci.cell_key(plan['cells'][-1]) + ': FAILED', text)
        self.assertNotIn(ci.cell_key(plan['cells'][0]) + ': PASSED', text)

    def test_deliverables_reject_symbols_only_and_empty_installers(self):
        os.environ['RUNNER_OS'] = 'macOS'
        root = Path('build-darwin-x86_64/newview')
        root.mkdir(parents=True)
        (root / 'symbols.bz2').write_bytes(b'symbols')
        with self.assertRaises(ValueError):
            ci.deliverables(None)
        (root / 'viewer.dmg').touch()
        with self.assertRaises(ValueError):
            ci.deliverables(None)
        (root / 'viewer.dmg').write_bytes(b'installer')
        ci.deliverables(None)
        self.assertEqual(ci.read_json('ci-results/deliverables.json')[0]['name'], 'viewer.dmg')

    def test_linux_runner_temp_archive_is_staged_for_upload(self):
        os.environ['RUNNER_OS'] = 'Linux'
        os.environ['RUNNER_TEMP'] = str(Path('runner-temp').resolve())
        Path('runner-temp').mkdir()
        Path('runner-temp/Phoenix-test.tar.xz').write_bytes(b'archive')
        Path('build-linux-x86_64/newview').mkdir(parents=True)
        ci.deliverables(None)
        self.assertTrue(Path('build-linux-x86_64/newview/Phoenix-test.tar.xz').is_file())
        self.assertFalse(Path('runner-temp/Phoenix-test.tar.xz').exists())

    def staged_gate(self, count):
        plan, records, jobs, artifacts = self.fixture(ci.matrix()[:count])
        gate = ci.check_results(plan, records, jobs, artifacts, 'success')
        for row in gate['results']:
            root = Path('downloaded-artifacts')
            if count > 1:
                root /= row['artifact']['name']
            root.mkdir(parents=True, exist_ok=True)
            file = root / 'Phoenix-test.dmg'
            file.write_bytes(b'installer')
            row['deliverables'] = [{'name': file.name, 'size': file.stat().st_size,
                                    'sha256': ci.file_digest(file)}]
        ci.write_json('gate.json', gate)
        return gate

    def test_single_artifact_download_layout(self):
        gate = self.staged_gate(1)
        ci.stage(argparse.Namespace(gate='gate.json'))
        self.assertTrue((Path('to_deploy') / gate['results'][0]['artifact']['name'] / 'Phoenix-test.dmg').is_file())

    def test_multiple_artifact_download_layout(self):
        gate = self.staged_gate(2)
        ci.stage(argparse.Namespace(gate='gate.json'))
        for row in gate['results']:
            self.assertTrue((Path('to_deploy') / row['artifact']['name'] / 'Phoenix-test.dmg').is_file())

    def test_changed_or_missing_installer_cannot_be_deployed(self):
        self.staged_gate(1)
        Path('downloaded-artifacts/Phoenix-test.dmg').write_bytes(b'tampered!')
        with self.assertRaises(ValueError):
            ci.stage(argparse.Namespace(gate='gate.json'))

    def test_manual_source_requires_current_successful_gate(self):
        plan, records, jobs, artifacts = self.fixture()
        gate = ci.check_results(plan, records, jobs, artifacts, 'success')
        ci.write_json('source-gate/ci-gate.json', gate)
        os.environ['SOURCE_RUN_ID'] = '42'
        run = {'id': 42, 'head_sha': 'abc123', 'run_attempt': 2,
               'path': '.github/workflows/build_viewer.yml'}
        for outcome in ('success', 'failure', 'cancelled'):
            with self.subTest(outcome=outcome), patch.object(ci, 'github', return_value=run):
                with patch.object(ci, 'pages', side_effect=[
                        [{'name': 'CI gate', 'conclusion': outcome}], artifacts]):
                    if outcome == 'success':
                        ci.verify_source(None)
                    else:
                        with self.assertRaises(ValueError):
                            ci.verify_source(None)
        run['run_attempt'] = 3
        with patch.object(ci, 'github', return_value=run), self.assertRaises(ValueError):
            ci.verify_source(None)

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ctest'), 'CMake required')
    def test_real_ctest_pass_failure_and_zero_test_runs(self):
        for case in ('pass', 'fail', 'empty'):
            with self.subTest(case=case):
                source = Path(case)
                source.mkdir()
                (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(CIHarness NONE)
option(LL_TESTS "Tests" ON)
enable_testing()
add_custom_target(BUILD_TESTS)
''' + (f'add_test(NAME sample COMMAND "${{CMAKE_COMMAND}}" -E {"true" if case == "pass" else "false"})\n' if case != 'empty' else ''))
                subprocess.run(['cmake', '-S', str(source), '-B', 'build-check'],
                               check=True, stdout=subprocess.DEVNULL)
                # Remove the preceding run's XML to also exercise missing-report behaviour.
                shutil.rmtree('ci-results', ignore_errors=True)
                captured = io.StringIO()
                with redirect_stdout(captured):
                    if case == 'pass':
                        ci.run_tests(argparse.Namespace(timeout=5))
                    else:
                        with self.assertRaises(ValueError):
                            ci.run_tests(argparse.Namespace(timeout=5))
                self.assertEqual(ci.read_json('ci-results/tests.json')['state'],
                                 'passed' if case == 'pass' else 'failed')
                shutil.rmtree('build-check')


class CISigning(Workspace):
    def test_signing_policy(self):
        for channel in ci.CHANNELS:
            with self.subTest(channel=channel):
                self.assertEqual(ci.signing_required(channel), channel in ('Release', 'Beta'))
                self.assertTrue(ci.signing_required(channel, override=True))

    def test_prepare_records_the_same_signing_policy_for_builds_and_gate(self):
        version = Path('indra/newview/VIEWER_VERSION_FS.txt')
        version.parent.mkdir(parents=True)
        version.write_text('7.3.0')
        for event, branch, override, expected in (
                ('push', 'Firestorm7.3.0', 'false', True),
                ('push', '7.3preview', 'false', True),
                ('schedule', 'Firestorm7.3.0', 'false', False),
                ('workflow_dispatch', 'Firestorm7.3.0', 'false', False),
                ('workflow_dispatch', 'Firestorm7.3.0', 'true', True)):
            os.environ.update(GITHUB_EVENT_NAME=event, GITHUB_REF_NAME=branch, OVERRIDE_SIGNING=override)
            Path(os.environ['GITHUB_OUTPUT']).write_text('')
            with self.subTest(event=event, branch=branch, override=override):
                with patch.object(ci.subprocess, 'check_output', return_value='123'):
                    ci.prepare(argparse.Namespace(mac_only=False))
                self.assertEqual(ci.read_json('ci-plan.json')['signing_required'], expected)
                self.assertIn('signing_required=' + str(expected).lower(),
                              Path(os.environ['GITHUB_OUTPUT']).read_text().splitlines())

    def test_unsigned_ci_builds_ignore_inherited_signing_configuration(self):
        for platform in ('Windows', 'macOS'):
            for channel in ('Nightly', 'Manual', 'Profiling', 'Alpha'):
                os.environ.update(RUNNER_OS=platform, FS_RELEASE_TYPE=channel,
                                  CI_SIGNING_REQUIRED='false', SIGNTOOL_PATH='signtool',
                                  CODESIGNING_DLIB_PATH='signing.dll', CODESIGNING_METADATA_PATH='metadata.json',
                                  build_secrets_checkout='signing')
                with self.subTest(platform=platform, channel=channel), patch.object(ci.subprocess, 'run') as run:
                    ci.build(None)
                    env = run.call_args.kwargs['env']
                    self.assertEqual(env['CI_SIGNING_REQUIRED'], 'false')
                    for name in ('SIGNTOOL_PATH', 'CODESIGNING_DLIB_PATH', 'CODESIGNING_METADATA_PATH'):
                        self.assertNotIn(name, env)
                    if platform == 'macOS':
                        self.assertNotIn('build_secrets_checkout', {key.lower() for key in env})
                    self.assertEqual(os.environ['SIGNTOOL_PATH'], 'signtool')

    def test_required_windows_builds_reject_missing_configuration_before_build(self):
        for channel in ('Release', 'Beta', 'Manual'):
            os.environ.update(RUNNER_OS='Windows', FS_RELEASE_TYPE=channel,
                              CI_SIGNING_REQUIRED='true' if channel == 'Manual' else 'false')
            with self.subTest(channel=channel), patch.object(ci.subprocess, 'run') as run:
                with self.assertRaises(ValueError):
                    ci.build(None)
                run.assert_not_called()

    def test_required_macos_build_cannot_fall_back_to_ad_hoc_signing(self):
        os.environ.update(RUNNER_OS='macOS', FS_RELEASE_TYPE='Release', CI_SIGNING_REQUIRED='false')
        with patch.object(ci.subprocess, 'run') as run, self.assertRaises(ValueError):
            ci.build(None)
        run.assert_not_called()

    def test_required_ci_builds_preserve_signing_configuration(self):
        secrets = Path('signing/code-signing-osx')
        secrets.mkdir(parents=True)
        (secrets / 'password.txt').write_text('fixture')
        (secrets / 'notarize_creds.sh').write_text('fixture')
        for platform in ('Windows', 'macOS'):
            for channel, override in (('Release', 'false'), ('Beta', 'false'), ('Manual', 'true')):
                os.environ.update(RUNNER_OS=platform, FS_RELEASE_TYPE=channel,
                                  CI_SIGNING_REQUIRED=override, SIGNTOOL_PATH='signtool',
                                  CODESIGNING_DLIB_PATH='signing.dll', CODESIGNING_METADATA_PATH='metadata.json',
                                  build_secrets_checkout='signing', addrsize='64')
                with self.subTest(platform=platform, channel=channel), patch.object(ci.subprocess, 'run') as run:
                    ci.build(None)
                    self.assertEqual(run.call_args.args[0],
                                     ['autobuild', 'build', '-c', 'ReleaseFS', '-A64', '--no-configure'])
                    self.assertTrue(run.call_args.kwargs['check'])
                    env = run.call_args.kwargs['env']
                    self.assertEqual(env['CI_SIGNING_REQUIRED'], 'true')
                    normalized = {key.lower(): value for key, value in env.items()}
                    for name in ('SIGNTOOL_PATH', 'CODESIGNING_DLIB_PATH',
                                 'CODESIGNING_METADATA_PATH', 'build_secrets_checkout'):
                        self.assertEqual(normalized[name.lower()], os.environ[name])

    def windows_package(self, channel='Release', grid='sl'):
        os.environ.update(RUNNER_OS='Windows', FS_RELEASE_TYPE=channel,
                          CI_SIGNING_REQUIRED='false', SIGNTOOL_PATH='signtool',
                          CODESIGNING_DLIB_PATH='signing.dll', CODESIGNING_METADATA_PATH='metadata.json',
                          CI_CELL=json.dumps({'grid': grid}))
        root = Path('build-vc170-64/newview/Release')
        root.mkdir(parents=True, exist_ok=True)
        ci.write_json(root / 'build_data.json', {'Executable': 'Firestorm.exe'})
        names = ['Firestorm.exe', 'slplugin.exe', 'llwebrtc.dll', 'llplugin/dullahan_host.exe', 'ViewerSetup.exe']
        if grid == 'os':
            names.append('SLVoice.exe')
        for name in names:
            file = root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'fixture')
        return root, names

    def test_required_windows_deliverables_verify_installer_and_all_signing_targets(self):
        for grid in ('sl', 'os'):
            with self.subTest(grid=grid):
                root, names = self.windows_package(grid=grid)
                with patch.object(ci.subprocess, 'run') as run:
                    ci.deliverables(None)
                verified = {Path(call.args[0][-1]).relative_to(root).as_posix()
                            for call in run.call_args_list}
                self.assertEqual(verified, set(names))
                self.assertTrue(all(call.args[0][:4] == ['signtool', 'verify', '/pa', '/all']
                                    and call.kwargs['check'] for call in run.call_args_list))
                self.assertTrue(ci.read_json('ci-results/deliverables.json')[0]['signature_verified'])

    def test_unsigned_or_invalid_windows_signatures_block_deliverables(self):
        self.windows_package(channel='Beta')
        with patch.object(ci.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'signtool')):
            with self.assertRaises(ValueError):
                ci.deliverables(None)
        self.assertFalse(Path('ci-results/deliverables.json').exists())

    def test_missing_required_windows_binary_blocks_deliverables(self):
        root, _ = self.windows_package(grid='os')
        (root / 'SLVoice.exe').unlink()
        with patch.object(ci.subprocess, 'run'), self.assertRaises(ValueError):
            ci.deliverables(None)

    def test_nightly_deliverables_do_not_require_signatures_unless_overridden(self):
        self.windows_package(channel='Nightly')
        with patch.object(ci.subprocess, 'run') as run:
            ci.deliverables(None)
            run.assert_not_called()
        os.environ['CI_SIGNING_REQUIRED'] = 'true'
        with patch.object(ci.subprocess, 'run') as run:
            ci.deliverables(None)
            self.assertEqual(run.call_count, 5)

    def test_gate_rejects_unverified_release_beta_and_override_windows_artifacts(self):
        for channel, override in (('Release', False), ('Beta', False), ('Manual', True)):
            plan, records, jobs, artifacts = self.fixture([{'os': 'windows-2022', 'grid': 'sl', 'variant': 'avx'}])
            plan.update(release_type=channel, signing_required=override)
            artifacts[0]['name'] = ci.artifact_name(channel, plan['cells'][0])
            with self.subTest(channel=channel):
                gate = ci.check_results(plan, records, jobs, artifacts, 'success')
                self.assertFalse(gate['passed'])
                self.assertIn('Required Windows signatures were not verified', ci.report_text(gate))
                records[0]['deliverables'][0]['signature_verified'] = True
                self.assertTrue(ci.check_results(plan, records, jobs, artifacts, 'success')['passed'])


class Signing(Workspace):
    def setUp(self):
        super().setUp()
        self.manifest = manifest_module.FSViewerManifest()
        self.manifest.args = {'configuration': 'Release', 'viewer_flavor': 'hvk'}
        self.manifest.final_exe = lambda: 'viewer.exe'

    def test_windows_missing_tools_skip_signing(self):
        for policy in (None, 'true', 'false'):
            for config in ({}, {'SIGNTOOL_PATH': 'signtool'},
                           {'CODESIGNING_DLIB_PATH': 'signing.dll',
                            'CODESIGNING_METADATA_PATH': 'metadata.json'}):
                env = dict(config)
                if policy is not None:
                    env['CI_SIGNING_REQUIRED'] = policy
                with self.subTest(policy=policy, config=config), patch.dict(os.environ, env, clear=True):
                    captured = io.StringIO()
                    with patch.object(manifest_module.subprocess, 'check_call') as sign, redirect_stdout(captured):
                        self.manifest.fs_sign_win_binaries()
                        self.manifest.fs_sign_win_installer({'installer_file': 'Setup.exe'})
                    sign.assert_not_called()
                    self.assertEqual(captured.getvalue().count('Skipping signing process.'), 2)

    def test_windows_signing_attempts_all_configured_files(self):
        for policy in (None, 'true', 'false'):
            for flavor in ('hvk', 'oss'):
                env = dict(SIGNTOOL_PATH='signtool', CODESIGNING_DLIB_PATH='signing.dll',
                           CODESIGNING_METADATA_PATH='metadata.json')
                if policy is not None:
                    env['CI_SIGNING_REQUIRED'] = policy
                self.manifest.args['viewer_flavor'] = flavor
                with self.subTest(policy=policy, flavor=flavor), patch.dict(os.environ, env, clear=True):
                    with patch.object(manifest_module.subprocess, 'check_call') as sign, redirect_stdout(io.StringIO()):
                        self.manifest.fs_sign_win_binaries()
                        self.manifest.fs_sign_win_installer({'installer_file': 'Setup.exe'})
                    expected = ['Release\\slplugin.exe', 'Release\\llwebrtc.dll',
                                'Release\\llplugin\\dullahan_host.exe', 'Release\\viewer.exe']
                    if flavor == 'oss':
                        expected.append('Release\\SLVoice.exe')
                    expected.append('Release\\Setup.exe')
                    self.assertEqual([call.args[0][-1] for call in sign.call_args_list], expected)
                    for call in sign.call_args_list:
                        self.assertEqual(call.args[0][:2], ['signtool', 'sign'])
                        self.assertEqual(call.args[0][-5:-1],
                                         ['/dlib', 'signing.dll', '/dmdf', 'metadata.json'])

    def test_windows_signing_errors_are_reported_without_aborting(self):
        os.environ.update(CI_SIGNING_REQUIRED='true', SIGNTOOL_PATH='signtool',
                          CODESIGNING_DLIB_PATH='signing.dll', CODESIGNING_METADATA_PATH='metadata.json')
        for error in (subprocess.CalledProcessError(1, 'signtool'), OSError('tool unavailable'),
                      TypeError('invalid signing argument')):
            with self.subTest(error=type(error).__name__):
                captured = io.StringIO()
                with patch.object(manifest_module.subprocess, 'check_call', side_effect=error) as sign:
                    with redirect_stdout(captured):
                        self.manifest.fs_sign_win_binaries()
                        self.manifest.fs_sign_win_installer({'installer_file': 'Setup.exe'})
                self.assertEqual(sign.call_count, 5)
                self.assertEqual(captured.getvalue().count("Couldn't sign binary:"), 4)
                self.assertIn("Couldn't sign windows installer: Release\\Setup.exe", captured.getvalue())

    def notarize(self, status='Accepted', submit_exit='0', staple_fail='false'):
        bins = Path('bin')
        bins.mkdir(exist_ok=True)
        commands = {
            'ditto': '#!/bin/bash\ntouch "${@: -1}"\n',
            'sleep': '#!/bin/sh\nexit 0\n',
            'xcrun': '''#!/bin/bash
if [[ $1 == notarytool ]]; then
    echo '{"id":"submission-id","status":"'"$MOCK_STATUS"'"}'
    exit "$MOCK_SUBMIT_EXIT"
fi
echo staple >> "$STAPLE_LOG"
[[ $MOCK_STAPLE_FAIL != true ]]
'''}
        for name, content in commands.items():
            path = bins / name
            path.write_text(content)
            path.chmod(0o755)
        config = Path('secrets/code-signing-osx')
        config.mkdir(parents=True, exist_ok=True)
        (config / 'notarize_creds.sh').write_text('USERNAME=test\nPASSWORD=test\nASC_PROVIDER=test\n')
        Path('Viewer.app').mkdir(exist_ok=True)
        env = dict(os.environ, PATH=str(bins.resolve()) + os.pathsep + os.environ['PATH'],
                   PYTHON=sys.executable, build_secrets_checkout=str(Path('secrets').resolve()),
                   MOCK_STATUS=status, MOCK_SUBMIT_EXIT=submit_exit,
                   MOCK_STAPLE_FAIL=staple_fail, STAPLE_LOG=str(Path('staples').resolve()))
        return subprocess.run(['bash', str(ROOT / 'indra/newview/installers/darwin/apple-notarize.sh'),
                               str(Path('Viewer.app').resolve())], env=env, capture_output=True, text=True)

    def test_rejected_notarization_id_is_not_acceptance(self):
        self.assertNotEqual(self.notarize(status='Invalid').returncode, 0)
        self.assertFalse(Path('staples').exists())

    def test_notarytool_failure_is_fatal(self):
        self.assertEqual(self.notarize(submit_exit='7').returncode, 7)
        self.assertFalse(Path('staples').exists())

    def test_accepted_notarization_is_stapled(self):
        self.assertEqual(self.notarize().returncode, 0)
        self.assertEqual(Path('staples').read_text().splitlines(), ['staple'])
        self.assertFalse(Path('Viewer.zip').exists())

    def test_stapling_exhaustion_is_fatal(self):
        self.assertEqual(self.notarize(staple_fail='true').returncode, 65)
        self.assertEqual(len(Path('staples').read_text().splitlines()), 5)

    def test_required_notarization_cannot_be_skipped(self):
        os.environ.update(SKIP_NOTARIZATION='true', CI_SIGNING_REQUIRED='true')
        self.assertNotEqual(self.notarize().returncode, 0)


if __name__ == '__main__':
    unittest.main()
