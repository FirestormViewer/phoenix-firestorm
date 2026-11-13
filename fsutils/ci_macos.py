#!/usr/bin/env python3
"""Reclaim space on disposable macOS CI runners before a universal build."""

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess


MOBILE_PLATFORMS = (
    'iPhoneOS', 'iPhoneSimulator', 'AppleTVOS', 'AppleTVSimulator',
    'WatchOS', 'WatchSimulator', 'XROS', 'XRSimulator',
)


def cleanup_paths(developer, applications=Path('/Applications')):
    """Resolve Xcode aliases before choosing installations to remove."""
    developer = developer.resolve(strict=True)
    selected = developer.parent.parent
    if (developer.name != 'Developer' or developer.parent.name != 'Contents'
            or selected.suffix != '.app' or selected.parent != applications.resolve()):
        raise ValueError(f'Expected an Xcode developer directory inside {applications}: {developer}')
    paths = [path for path in sorted(applications.glob('Xcode*.app'))
             if path.resolve() != selected]
    paths.extend(developer / 'Platforms' / (name + '.platform') for name in MOBILE_PLATFORMS)
    return developer, paths


def report():
    subprocess.run(['df', '-h', '.'], check=True)
    paths = []
    for root in sorted(Path('.').glob('build-darwin-*')):
        paths.extend(path for path in (root, root / 'build', root / 'build/LTOCache',
                                      root / 'packages', root / 'newview', root / 'symbols')
                     if path.exists())
    if paths:
        subprocess.run(['du', '-sh', *map(str, paths)], check=False)


def prepare():
    requested = os.environ.get('DEVELOPER_DIR') or subprocess.check_output(
        ['xcode-select', '--print-path'], text=True).strip()
    developer = Path(requested)
    if developer.suffix == '.app':
        developer /= 'Contents/Developer'
    developer, paths = cleanup_paths(developer)
    # Use this exact installation throughout the job, even through Xcode.app aliases.
    os.environ['DEVELOPER_DIR'] = str(developer)
    sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    before = shutil.disk_usage('.').free
    print(f'Keeping Xcode: {developer}\nKeeping macOS SDK: {sdk}', flush=True)
    report()
    for path in paths:
        if path.exists() or path.is_symlink():
            subprocess.run(['du', '-sh', str(path)], check=False)
            subprocess.run(['sudo', 'rm', '-rf', '--', str(path)], check=True)
    # Bottles can be discarded after Bash and GNU sed have been installed.
    bottles = Path.home() / 'Library/Caches/Homebrew'
    if bottles.is_dir() and not bottles.is_symlink():
        shutil.rmtree(bottles)
    # Check the retained toolchain before spending time configuring/building.
    subprocess.run(['xcodebuild', '-version'], check=True)
    subprocess.run(['xcrun', '--sdk', 'macosx', '--find', 'clang++'], check=True)
    if not Path(sdk).is_dir():
        raise ValueError('macOS SDK is missing after runner cleanup')
    for architecture in ('arm64', 'x86_64'):
        subprocess.run(['xcrun', '--sdk', 'macosx', 'clang++', '-isysroot', sdk,
                        '-arch', architecture, '-std=c++20', '-fsyntax-only', '-x', 'c++', '-'],
                       input='#include <filesystem>\n#include <CoreFoundation/CoreFoundation.h>\n',
                       text=True, check=True)
    cache = Path(os.environ['RUNNER_TEMP']) / 'firestorm-autobuild-cache'
    with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as stream:
        stream.write(f'DEVELOPER_DIR={developer}\nAUTOBUILD_INSTALLABLE_CACHE={cache}\n')
    print(f'Reclaimed {(shutil.disk_usage(".").free - before) / 1024**3:.1f} GiB', flush=True)
    report()


def after_configure():
    # CMake has installed the dependencies into build-darwin-*/packages.
    # Only the downloaded archives in our own cache can now be discarded.
    expected = Path(os.environ['RUNNER_TEMP']) / 'firestorm-autobuild-cache'
    cache = Path(os.environ['AUTOBUILD_INSTALLABLE_CACHE'])
    if cache != expected or cache.is_symlink():
        raise ValueError('Refusing to remove an unexpected autobuild cache')
    if cache.is_dir():
        subprocess.run(['du', '-sh', str(cache)], check=False)
        shutil.rmtree(cache)
    report()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('prepare', 'after-configure', 'report'))
    args = parser.parse_args()
    if platform.system() != 'Darwin' or os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('This helper is only for macOS GitHub Actions runners')
    if args.phase != 'report' and os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted':
        parser.error('Cleanup is restricted to disposable GitHub-hosted runners')
    {'prepare': prepare, 'after-configure': after_configure, 'report': report}[args.phase]()


if __name__ == '__main__':
    main()
