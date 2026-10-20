#!/usr/bin/env python3
"""Check a candidate kit against both faces, using isolated temporary snapshots.

Uses local consumer source (including uncommitted changes) and never changes
their kit checkout. Node dependencies must already be installed in each face.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

KIT = Path(__file__).resolve().parents[1]


def check(project, build=True, render=False):
    project = project.resolve()
    with tempfile.TemporaryDirectory(prefix=project.name+'-kit-check-') as temporary:
        target = Path(temporary)/project.name
        shutil.copytree(project, target, symlinks=True, ignore=shutil.ignore_patterns(
            '.git', 'shared-kit', 'node_modules', 'build', 'shots', '__pycache__', '.waf-*', '.lock-waf*'))
        if not (target/'src/c/kit').is_symlink():
            raise RuntimeError(str(project)+' has not been migrated to the shared kit')
        # Build provenance needs a real revision. This local clone carries history;
        # then overlay candidate files to include edits in a truthful dirty tree.
        subprocess.run(['git', 'clone', '--quiet', '--no-hardlinks', str(KIT), str(target/'shared-kit')], check=True)
        # Match deletions as well as edits without retaining the original .git file.
        for path in (target/'shared-kit').iterdir():
            if path.name == '.git':
                continue
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
        shutil.copytree(KIT, target/'shared-kit', dirs_exist_ok=True, symlinks=True,
                        ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
        if (project/'node_modules').exists():
            (target/'node_modules').symlink_to(project/'node_modules', target_is_directory=True)
        (target/'shots').mkdir()
        subprocess.run([sys.executable, 'tests/run.py'], cwd=target, check=True)
        if build or render:
            subprocess.run(['pebble', 'build'], cwd=target, check=True)
        if render:
            env = os.environ.copy()
            env.setdefault('WATCHFACE_LAB', str(project.parent/'watchface-lab'))
            subprocess.run([sys.executable, 'tools/render_scenes.py'], cwd=target, env=env, check=True)
        print('Passed candidate kit against '+project.name, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('projects', type=Path, nargs='*')
    parser.add_argument('--tests-only', action='store_true')
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    projects = args.projects or [KIT.parent/'pebble-mondrian', KIT.parent/'pebble-underground']
    for project in projects:
        check(project, not args.tests_only, args.render)
