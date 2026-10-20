#!/usr/bin/env python3
"""Consumer release entry point: update the kit, then run its release builder."""
import argparse
from pathlib import Path
import subprocess
import sys


def update(project):
    kit = project/'shared-kit'
    def git(root, *args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()
    if (kit/'.git').exists() and git(kit, 'status', '--porcelain'):
        raise RuntimeError('Shared kit has unfinished edits. Commit/share them, or use pebble build for development.')
    if not (kit/'.git').exists():
        subprocess.run(['git', 'submodule', 'update', '--init', '--', 'shared-kit'], cwd=project, check=True)
    # Fetch first, then refuse to abandon locally committed kit work.
    subprocess.run(['git', '-C', str(kit), 'fetch', 'origin', 'main'], check=True)
    ahead = subprocess.run(['git', '-C', str(kit), 'merge-base', '--is-ancestor', 'HEAD', 'origin/main'])
    if ahead.returncode:
        raise RuntimeError('The kit contains commits outside origin/main. Share/integrate them before updating.')
    subprocess.run(['git', 'submodule', 'update', '--remote', '--no-fetch', '--', 'shared-kit'], cwd=project, check=True)


def main(project):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pinned', action='store_true', help='Use the recorded kit checkout without network access')
    parser.add_argument('--update-only', action='store_true', help='Update the kit without building')
    parser.add_argument('--render', action='store_true', help='Also run this face\'s render coverage (requires Watchface Lab)')
    args = parser.parse_args()
    project = Path(project).resolve()
    if not args.pinned:
        update(project)
    if not args.update_only:
        command = [sys.executable, str(project/'shared-kit/tools/release.py'), str(project)]
        if args.render:
            command.append('--render')
        subprocess.run(command, cwd=project, check=True)


if __name__ == '__main__':
    main(Path.cwd())
