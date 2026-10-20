#!/usr/bin/env python3
"""Build a tested live PBW and a receipt binding it to its kit revision."""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def release(project, render=False):
    project = Path(project).resolve()
    kit = project/'shared-kit'
    if git(kit, 'status', '--porcelain'):
        raise RuntimeError('Commit and share the kit changes before making a release.')
    revision = git(kit, 'rev-parse', 'HEAD')
    package = json.loads((project/'package.json').read_text())
    env = os.environ.copy()
    env['LAB_SCENARIO'] = '3'
    env['KIT_TOUCH_WINDOW_MS'] = '0'
    def run(*args):
        subprocess.run(args, cwd=project, env=env, check=True)
    run(sys.executable, 'tests/run.py')
    run('pebble', 'clean')
    run('pebble', 'build')
    if render:
        run(sys.executable, 'tools/render_scenes.py')
    artifact = project/'build'/(package['name']+'.pbw')
    with zipfile.ZipFile(artifact) as archive:
        info = json.loads(archive.read('appinfo.json'))
    assert info['uuid'] == package['pebble']['uuid'], 'PBW UUID differs from package.json'
    assert info['versionLabel'] == package['version'], 'PBW version differs from package.json'
    assert revision == git(kit, 'rev-parse', 'HEAD') and not git(kit, 'status', '--porcelain'), 'Kit changed during release'
    receipt = {
        'created_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
        'app': package['name'], 'version': package['version'], 'uuid': info['uuid'],
        'app_revision': git(project, 'rev-parse', 'HEAD'),
        'app_modified': bool(git(project, 'status', '--porcelain')),
        'kit_version': (kit/'VERSION').read_text().strip(), 'kit_revision': revision,
        'kit_repository': git(kit, 'remote', 'get-url', 'origin'),
        'pbw_sha256': hashlib.sha256(artifact.read_bytes()).hexdigest(),
        'scenario': 3, 'touch_window_ms': 0,
        'tests_passed': True, 'renders_checked': render,
    }
    receipt_path = artifact.with_suffix('.provenance.json')
    receipt_path.write_text(json.dumps(receipt, indent=2)+'\n')
    print('Built {}\nKit {}\nReceipt {}'.format(artifact, revision, receipt_path), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    release(args.project, args.render)
