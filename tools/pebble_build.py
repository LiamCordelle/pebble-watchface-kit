"""Shared Waf implementation. Consumer wscript files load this module."""
import json
import os
from pathlib import Path
import subprocess

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')
    scenario = int(os.environ.get('LAB_SCENARIO', '3'))
    touch_window = int(os.environ.get('KIT_TOUCH_WINDOW_MS', '0'))
    if scenario not in (0, 1, 2, 3) or not 0 <= touch_window <= 60000:
        ctx.fatal('Invalid LAB_SCENARIO or KIT_TOUCH_WINDOW_MS')
    project = Path(ctx.path.abspath())
    kit = project / 'shared-kit'
    revision = subprocess.check_output(['git', '-C', str(kit), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', str(kit), 'status', '--porcelain'], text=True).strip())
    print('Watchface kit {} @ {}{}'.format((kit/'VERSION').read_text().strip(), revision, ' (modified)' if dirty else ''))
    (project/'build').mkdir(exist_ok=True)
    (project/'build/kit-build.json').write_text(json.dumps({
        'version': (kit/'VERSION').read_text().strip(), 'revision': revision,
        'modified': dirty, 'scenario': scenario, 'touch_window_ms': touch_window,
    }, indent=2)+'\n')
    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('DEFINES', 'LAB_SCENARIO={}'.format(scenario))
        ctx.env.append_value('DEFINES', 'KIT_TOUCH_WINDOW_MS={}'.format(touch_window))
        ctx.env.append_value('LINKFLAGS', '-Wl,--no-warn-rwx-segments')
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env
    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js', 'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')
