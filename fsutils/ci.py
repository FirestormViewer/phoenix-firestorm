#!/usr/bin/env python3
"""Firestorm CI policy, test execution, reporting and deployment verification.

Only the Python standard library is used so reporting works without build deps.
GitHub and Discord credentials are read from the environment, never CLI arguments.
"""

import argparse
from collections import Counter
import hashlib
import itertools
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
import xml.etree.ElementTree as ET


CHANNELS = {
    'Release': ('release', 'RELEASE_WEBHOOK_URL'),
    'Beta': ('preview', 'BETA_WEBHOOK_URL'),
    'Alpha': ('test', 'BETA_WEBHOOK_URL'),
    'Nightly': ('nightly', 'NIGHTLY_WEBHOOK_URL'),
    'Manual': ('test', 'MANUAL_WEBHOOK_URL'),
    'Profiling': ('test', 'MANUAL_WEBHOOK_URL'),
}
REQUIRED_STEPS = ('configure', 'build', 'tests', 'deliverables')


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def outputs(**values):
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
        for name, value in values.items():
            if not isinstance(value, str):
                value = json.dumps(value, separators=(',', ':'))
            if '\n' in value or '\r' in value:
                raise ValueError('Multiline workflow output is not allowed')
            stream.write(f'{name}={value}\n')


def release_type(event, branch, profiling=False):
    # An explicitly dispatched build is ad-hoc even on a release branch.
    if event == 'workflow_dispatch':
        return 'Profiling' if profiling else 'Manual'
    if event == 'schedule':
        return 'Nightly'
    if branch.startswith('Firestorm'):
        return 'Release'
    if 'preview' in branch:
        return 'Beta'
    if 'alpha' in branch:
        return 'Alpha'
    if 'nightly' in branch:
        return 'Nightly'
    raise ValueError(f'Cannot determine release type for {event}: {branch}')


def signing_required(channel, override=False):
    return channel in ('Release', 'Beta') or override


def build(args):
    """Apply CI signing policy without changing the local packaging defaults."""
    env = dict(os.environ)
    required = signing_required(env['FS_RELEASE_TYPE'], env.get('CI_SIGNING_REQUIRED') == 'true')
    env['CI_SIGNING_REQUIRED'] = 'true' if required else 'false'
    if env['RUNNER_OS'] in ('Windows', 'macOS'):
        if not required:
            for name in ('SIGNTOOL_PATH', 'CODESIGNING_DLIB_PATH', 'CODESIGNING_METADATA_PATH'):
                env.pop(name, None)
            if env['RUNNER_OS'] == 'macOS':
                # The existing packager then uses ad hoc signing for Apple silicon.
                env.pop('build_secrets_checkout', None)
                env.pop('BUILD_SECRETS_CHECKOUT', None)
        elif env['RUNNER_OS'] == 'Windows':
            if not all(env.get(name) for name in (
                    'SIGNTOOL_PATH', 'CODESIGNING_DLIB_PATH', 'CODESIGNING_METADATA_PATH')):
                raise ValueError('Required Windows signing configuration is incomplete')
        else:
            secrets = os.environ.get('build_secrets_checkout')
            if not secrets or not all((Path(secrets) / 'code-signing-osx' / name).is_file()
                                      for name in ('password.txt', 'notarize_creds.sh')):
                raise ValueError('Required macOS signing configuration is incomplete')
    subprocess.run(['autobuild', 'build', '-c', 'ReleaseFS', '-A' + os.environ.get('addrsize', '64'),
                    '--no-configure'], env=env, check=True)


def matrix(mac_only=False):
    if mac_only:
        return [{'os': 'macos-15', 'grid': 'sl', 'variant': 'avx'}]
    return [dict(zip(('os', 'grid', 'variant'), values)) for values in
            itertools.product(('macos-15', 'ubuntu-24.04', 'windows-2022'),
                              ('sl', 'os'), ('regular', 'avx'))]


def cell_key(cell):
    return '/'.join(cell[key] for key in ('os', 'grid', 'variant'))


def artifact_name(channel, cell):
    return f"{channel}-{cell['os']}-{cell['variant']}-{cell['grid']}-artifacts.zip"


def identity():
    return {key: os.environ[name] for key, name in (
        ('repository', 'GITHUB_REPOSITORY'), ('sha', 'GITHUB_SHA'),
        ('run_id', 'GITHUB_RUN_ID'), ('attempt', 'GITHUB_RUN_ATTEMPT'))}


def prepare(args):
    channel = release_type(os.environ['GITHUB_EVENT_NAME'], os.environ['GITHUB_REF_NAME'],
                           os.environ.get('INCLUDE_TRACY') == 'true')
    cells = matrix(args.mac_only)
    version = Path('indra/newview/VIEWER_VERSION_FS.txt').read_text().strip()
    build = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'], text=True).strip()
    required = signing_required(channel, os.environ.get('OVERRIDE_SIGNING') == 'true')
    plan = dict(identity(), release_type=channel, signing_required=required,
                version=version, build=build, cells=cells)
    write_json('ci-plan.json', plan)
    outputs(release_type=channel, fail_fast=channel not in ('Nightly', 'Manual', 'Profiling'),
            signing_required=required, version=version, build=build, matrix={'include': cells})


def junit_summary(path):
    root = ET.parse(path).getroot()
    cases = list(root.iter('testcase'))
    failed = [case.get('name', 'unnamed') for case in cases
              if case.find('failure') is not None or case.find('error') is not None]
    skipped = sum(case.find('skipped') is not None for case in cases)
    if not cases or len(cases) == skipped:
        raise ValueError('No tests executed (empty or entirely skipped suite)')
    return {'total': len(cases), 'passed': len(cases) - len(failed) - skipped,
            'failed': len(failed), 'skipped': skipped, 'failures': failed}


def run_logged(command, log):
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, errors='replace') as process:
        for line in process.stdout:
            print(line, end='', flush=True)
            log.write(line)
        return process.wait()


def run_tests(args):
    result_dir = Path('ci-results').resolve()
    result_dir.mkdir(exist_ok=True)
    (result_dir / 'junit.xml').unlink(missing_ok=True)
    status = {'state': 'failed', 'detail': 'Test preparation failed'}
    try:
        caches = [path for path in Path('.').glob('build-*/CMakeCache.txt')
                  if 'LL_TESTS:BOOL=ON' in path.read_text(encoding='utf-8')]
        if len(caches) != 1:
            raise ValueError('Expected exactly one build tree with LL_TESTS=ON')
        build_dir = str(caches[0].parent.resolve())
        with (result_dir / 'tests.log').open('w', encoding='utf-8') as log:
            code = run_logged(['cmake', '--build', build_dir, '--config', 'Release',
                               '--target', 'BUILD_TESTS', '--parallel', '2'], log)
            if code:
                raise ValueError('Test executable compilation failed')
            code = run_logged(['ctest', '--test-dir', build_dir, '-C', 'Release',
                               '--parallel', '2', '--output-on-failure', '--no-tests=error',
                               '--timeout', str(args.timeout), '--output-junit',
                               str(result_dir / 'junit.xml')], log)
        summary = junit_summary(result_dir / 'junit.xml')
        status = dict(summary, state='passed' if code == 0 and summary['failed'] == 0 else 'failed',
                      detail='CTest completed' if code == 0 else f'CTest exited with status {code}')
    except (OSError, ValueError, ET.ParseError) as error:
        status['detail'] = str(error)
    write_json(result_dir / 'tests.json', status)
    if status['state'] != 'passed':
        raise ValueError(status['detail'])


def file_digest(path):
    with open(path, 'rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_windows_signatures(installer):
    root = installer.parent
    executable = read_json(root / 'build_data.json')['Executable']
    if Path(executable).name != executable or Path(executable).suffix.lower() != '.exe':
        raise ValueError('Invalid packaged viewer executable name')
    grid = json.loads(os.environ['CI_CELL'])['grid']
    if grid not in ('sl', 'os'):
        raise ValueError('Unknown viewer grid for Windows signature verification')
    targets = [installer, root / executable, root / 'slplugin.exe', root / 'llwebrtc.dll',
               root / 'llplugin/dullahan_host.exe']
    if grid == 'os':
        targets.append(root / 'SLVoice.exe')
    if not all(path.is_file() and path.stat().st_size > 0 for path in targets):
        raise ValueError('A required Windows signing target is missing or empty')
    tool = os.environ.get('SIGNTOOL_PATH')
    if not tool:
        raise ValueError('SignTool is required to verify Windows deliverables')
    for path in targets:
        try:
            subprocess.run([tool, 'verify', '/pa', '/all', str(path)], check=True)
        except subprocess.CalledProcessError as error:
            raise ValueError(f'Windows signature verification failed: {path.name}') from error


def deliverables(args):
    patterns = {
        'Windows': 'build-*/newview/Release/*Setup.exe',
        'macOS': 'build-darwin-*/newview/*.dmg',
        'Linux': 'build-linux-*/newview/Phoenix*.tar.xz',
    }
    files = sorted(path for path in Path('.').glob(patterns[os.environ['RUNNER_OS']])
                   if path.is_file() and path.stat().st_size > 0)
    if not files and os.environ['RUNNER_OS'] == 'Linux' and os.environ.get('RUNNER_TEMP'):
        temporary = [path for path in Path(os.environ['RUNNER_TEMP']).glob('Phoenix*.tar.xz')
                     if path.is_file() and path.stat().st_size > 0]
        roots = list(Path('.').glob('build-linux-*/newview'))
        if len(temporary) == 1 and len(roots) == 1:
            # viewer_manifest diverts GitHub's Linux archive to RUNNER_TEMP.
            target = roots[0] / temporary[0].name
            shutil.move(str(temporary[0]), str(target))
            files = [target]
    if len(files) != 1:
        raise ValueError(f'Expected one nonempty installer/archive; found {len(files)}')
    required = signing_required(os.environ.get('FS_RELEASE_TYPE'),
                                os.environ.get('CI_SIGNING_REQUIRED') == 'true')
    verified = os.environ['RUNNER_OS'] == 'Windows' and required
    if verified:
        verify_windows_signatures(files[0])
    write_json('ci-results/deliverables.json', [
        dict(name=path.name, size=path.stat().st_size, sha256=file_digest(path),
             **({'signature_verified': True} if verified else {})) for path in files])


def record(args):
    steps = json.loads(os.environ['CI_STEPS'])
    states = {name: step.get('outcome', 'unknown') for name, step in steps.items()}
    published = [step for name, step in steps.items() if name.startswith('publish_')
                 and step.get('outcome') == 'success']
    result = dict(identity(), cell=json.loads(os.environ['CI_CELL']), steps=states,
                  artifact_id=published[0].get('outputs', {}).get('artifact-id') if len(published) == 1 else None)
    for key in ('tests', 'deliverables'):
        path = Path('ci-results') / f'{key}.json'
        result[key] = read_json(path) if path.exists() else None
    write_json('ci-results/result.json', result)


def request_json(url, token=None, payload=None):
    headers = {'Accept': 'application/vnd.github+json' if token else 'application/json',
               'User-Agent': 'Firestorm-CI'}
    if token:
        headers['Authorization'] = f'Bearer {token}'
    data = None
    if payload is not None:
        data = json.dumps(payload).encode('utf-8')
        headers['Content-Type'] = 'application/json'
    for attempt in range(3):
        try:
            with urlopen(Request(url, data=data, headers=headers), timeout=30) as response:
                body = response.read()
                if payload is not None:
                    # Compatible webhook services may acknowledge with plain text.
                    return None
                return json.loads(body) if body else None
        except HTTPError as error:
            if error.code != 429 and error.code < 500:
                raise ValueError(f'Remote request failed (HTTP {error.code})') from None
            if attempt == 2:
                raise ValueError(f'Remote request failed after retries (HTTP {error.code})') from None
            time.sleep(min(float(error.headers.get('Retry-After', 2 ** attempt)), 10))
        except (URLError, TimeoutError):
            if attempt == 2:
                raise ValueError('Remote request failed after retries') from None
            time.sleep(2 ** attempt)


def github(path):
    base = os.environ.get('GITHUB_API_URL', 'https://api.github.com').rstrip('/')
    repo = os.environ['GITHUB_REPOSITORY']
    return request_json(f'{base}/repos/{repo}/{path}', os.environ['GH_TOKEN'])


def pages(path, key):
    separator = '&' if '?' in path else '?'
    for page in itertools.count(1):
        items = github(f'{path}{separator}per_page=100&page={page}')[key]
        yield from items
        if len(items) < 100:
            break


def check_results(plan, records, jobs, artifacts, matrix_result):
    """Require independently successful jobs, tests and immutable published artifacts."""
    current = {key: str(plan[key]) for key in ('repository', 'sha', 'run_id', 'attempt')}
    valid = {}
    expected_cells = {cell_key(cell) for cell in plan['cells']}
    for result in records:
        if any(str(result.get(key)) != value for key, value in current.items()):
            raise ValueError('Result belongs to a different commit, run or attempt')
        key = cell_key(result['cell'])
        if key not in expected_cells:
            raise ValueError(f'Unexpected result: {key}')
        if key in valid:
            raise ValueError(f'Duplicate result: {key}')
        valid[key] = result
    artifact_ids = {str(artifact['id']): artifact for artifact in artifacts}
    rows = []
    for cell in plan['cells']:
        key = cell_key(cell)
        matches = [job for job in jobs if job['name'] == f'Build / {key}']
        job = matches[0] if len(matches) == 1 else {}
        state = job.get('conclusion') or 'missing'
        failed_steps = [step['name'] for step in job.get('steps', [])
                        if step.get('conclusion') in ('failure', 'timed_out')]
        result = valid.get(key, {})
        detail = ', '.join(failed_steps) or state
        tests = result.get('tests') or {}
        deliverables = result.get('deliverables') or []
        requires_signatures = cell['os'].startswith('windows') and signing_required(
            plan['release_type'], plan.get('signing_required', False))
        signatures_ok = not requires_signatures or (
            bool(deliverables) and all(item.get('signature_verified') is True for item in deliverables))
        artifact = artifact_ids.get(str(result.get('artifact_id')), {})
        expected = artifact_name(plan['release_type'], cell)
        complete = (
            state == 'success'
            and all(result.get('steps', {}).get(step) == 'success' for step in REQUIRED_STEPS)
            and tests.get('state') == 'passed' and tests.get('total', 0) > 0
            and tests.get('passed', 0) > 0 and tests.get('failed') == 0
            and bool(deliverables) and signatures_ok
            and artifact.get('name') == expected and not artifact.get('expired', True)
            and artifact.get('size_in_bytes', 0) > 0)
        if complete:
            state, detail = 'passed', 'Build, tests and deliverable checks passed'
        elif state == 'success':
            state = 'failed'
            detail = ('Required Windows signatures were not verified' if not signatures_ok
                      else 'Incomplete results, tests or required artifact')
        elif state in ('failure', 'timed_out'):
            state = 'failed'
        rows.append({'cell': cell, 'state': state, 'detail': detail, 'tests': tests,
                     'url': job.get('html_url', ''), 'artifact': artifact,
                     'deliverables': deliverables})
    passed = matrix_result == 'success' and bool(rows) and all(row['state'] == 'passed' for row in rows)
    return dict(plan, passed=passed, matrix_result=matrix_result, results=rows)


def report_text(gate, failures_only=False):
    counts = Counter(row['state'] for row in gate['results'])
    channel = gate['release_type']
    lines = [f"{'✅' if gate['passed'] else '❌'} Firestorm {channel} — CI {'passed' if gate['passed'] else 'failed'}",
             'DEPLOYMENT ALLOWED' if gate['passed'] else 'DEPLOYMENT BLOCKED', '',
             f"Version: {gate['version']}.{gate['build']} · Commit: {gate['sha'][:12]}",
             'Build combinations: ' + ' · '.join(f'{count} {state}' for state, count in sorted(counts.items())), '']
    priority = {'failed': 0, 'unknown': 1, 'missing': 1, 'cancelled': 2, 'passed': 3}
    for row in sorted(gate['results'], key=lambda row: priority.get(row['state'], 1)):
        if failures_only and row['state'] == 'passed':
            continue
        lines.append(f"{cell_key(row['cell'])}: {row['state'].upper()} — {row['detail']}")
        tests = row['tests']
        if tests.get('total'):
            lines.append(f"  CTest: {tests['passed']} passed, {tests['failed']} failed, {tests['skipped']} skipped")
            lines.extend(f"  Failed test: {name}" for name in tests.get('failures', [])[:3])
        else:
            lines.append('  Tests: ' + tests.get('detail', 'not completed / no results'))
        if row['state'] != 'passed' and row['url']:
            lines.append('  ' + row['url'])
    return '\n'.join(lines)


def send_report(gate):
    hook = os.environ.get(CHANNELS[gate['release_type']][1])
    if not hook:
        raise ValueError('CI report webhook is not configured for this channel')
    url = f"{os.environ.get('GITHUB_SERVER_URL', 'https://github.com')}/{gate['repository']}/actions/runs/{gate['run_id']}/attempts/{gate['attempt']}"
    # Complete results remain in the GitHub summary. Discord gets a bounded digest.
    content = report_text(gate, failures_only=not gate['passed'])
    if len(content) > 1800:
        content = content[:1760] + '\n… remaining results in the full report.'
    content += '\n\nFull report and logs: ' + url
    request_json(hook, payload={'content': content[:2000], 'allowed_mentions': {'parse': []}})


def collect(args):
    plan = read_json('ci-plan.json')
    try:
        records = [read_json(path) for path in Path('ci-reports').glob('**/result.json')]
        jobs = list(pages(f"actions/runs/{plan['run_id']}/attempts/{plan['attempt']}/jobs", 'jobs'))
        artifacts = list(pages(f"actions/runs/{plan['run_id']}/artifacts", 'artifacts'))
        gate = check_results(plan, records, jobs, artifacts, os.environ['CI_MATRIX_RESULT'])
    except (ValueError, OSError, KeyError, TypeError, AttributeError) as error:
        gate = dict(plan, passed=False, matrix_result=os.environ['CI_MATRIX_RESULT'], results=[
            {'cell': cell, 'state': 'unknown', 'detail': 'Result collection failed', 'tests': {}, 'url': ''}
            for cell in plan['cells']])
        print(f'::error::CI result collection failed: {error}')
    write_json('ci-gate.json', gate)
    text = report_text(gate)
    print(text)
    with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
        summary.write(text + '\n')
    send_report(gate)
    if not gate['passed']:
        raise ValueError('CI gate rejected this build; deployment is blocked')
    outputs(artifact_ids=','.join(str(row['artifact']['id']) for row in gate['results']))


def resolve_source(args):
    number = int(os.environ['SOURCE_RUN_NUMBER'])
    for run in pages('actions/workflows/build_viewer.yml/runs', 'workflow_runs'):
        if run['run_number'] == number:
            outputs(run_id=str(run['id']), attempt=str(run['run_attempt']), sha=run['head_sha'])
            return
    raise ValueError('Source build run number was not found')


def verify_source(args):
    gate = read_json('source-gate/ci-gate.json')
    run_id = os.environ['SOURCE_RUN_ID']
    run = github(f'actions/runs/{run_id}')
    if (not gate.get('passed') or gate['repository'] != os.environ['GITHUB_REPOSITORY']
            or str(gate['run_id']) != str(run['id']) or gate['sha'] != run['head_sha']
            or str(gate['attempt']) != str(run['run_attempt'])
            or not run['path'].split('@')[0].endswith('/build_viewer.yml')):
        raise ValueError('Source gate does not match the latest source run attempt')
    jobs = list(pages(f"actions/runs/{run_id}/attempts/{gate['attempt']}/jobs", 'jobs'))
    gates = [job for job in jobs if job['name'] == 'CI gate']
    if len(gates) != 1 or gates[0].get('conclusion') != 'success':
        raise ValueError('Source run has no successful CI gate')
    artifacts = {str(item['id']): item for item in pages(f'actions/runs/{run_id}/artifacts', 'artifacts')}
    if (len(gate['results']) != len(matrix())
            or {cell_key(row['cell']) for row in gate['results']} != {cell_key(cell) for cell in matrix()}
            or any(row['state'] != 'passed' for row in gate['results'])):
        raise ValueError('Source gate has an incomplete build matrix')
    for row in gate['results']:
        artifact = artifacts.get(str(row['artifact']['id']), {})
        if artifact.get('expired', True) or artifact.get('name') != row['artifact']['name']:
            raise ValueError('A verified source artifact is missing or expired')
    outputs(artifact_ids=','.join(str(row['artifact']['id']) for row in gate['results']),
            release_type=gate['release_type'], version=gate['version'], build=gate['build'])


def stage(args):
    gate = read_json(args.gate)
    if not gate.get('passed') or not gate.get('results'):
        raise ValueError('A passing CI gate is required')
    source = Path('downloaded-artifacts')
    destination = Path('to_deploy')
    destination.mkdir(exist_ok=False)
    # download-artifact extracts a single artifact directly into its root.
    for row in gate['results']:
        name = row['artifact']['name']
        if Path(name).name != name or not name.endswith('-artifacts.zip'):
            raise ValueError('Invalid viewer artifact name')
        root = source / name if len(gate['results']) > 1 else source
        if not root.is_dir():
            raise ValueError(f'Missing downloaded artifact: {name}')
        if not row.get('deliverables'):
            raise ValueError(f'No verified installer/archive recorded: {name}')
        for expected in row['deliverables']:
            if Path(expected['name']).name != expected['name']:
                raise ValueError('Invalid deliverable name')
            found = [path for path in root.rglob(expected['name']) if path.is_file()]
            if (len(found) != 1 or found[0].stat().st_size != expected['size']
                    or file_digest(found[0]) != expected['sha256']):
                raise ValueError(f'Required deliverable is missing or changed: {expected["name"]}')
        # download_list accepts artifact directories ending in .zip as well as zip files.
        shutil.move(str(root), str(destination / name))


def select_hook(args):
    channel = os.environ['FS_RELEASE_TYPE']
    folder, secret = CHANNELS[channel]
    hook = os.environ.get(secret)
    if not hook:
        raise ValueError('Deployment webhook is not configured for this channel')
    print('::add-mask::' + hook)
    with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as stream:
        stream.write(f'FS_RELEASE_FOLDER={folder}\nFS_BUILD_WEBHOOK_URL={hook}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    sub = commands.add_parser('prepare')
    sub.add_argument('--mac-only', action='store_true')
    sub = commands.add_parser('test')
    sub.add_argument('--timeout', type=int, default=300)
    sub = commands.add_parser('stage')
    sub.add_argument('--gate', default='ci-gate.json')
    for name in ('build', 'deliverables', 'record', 'collect', 'resolve-source', 'verify-source', 'select-hook'):
        commands.add_parser(name)
    args = parser.parse_args()
    handlers = {'prepare': prepare, 'build': build, 'test': run_tests, 'deliverables': deliverables,
                'record': record, 'collect': collect, 'resolve-source': resolve_source,
                'verify-source': verify_source, 'stage': stage, 'select-hook': select_hook}
    try:
        handlers[args.command](args)
    except (ValueError, OSError, KeyError, ET.ParseError, subprocess.CalledProcessError) as error:
        print(f'::error::{error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
