#!/usr/bin/env python3
"""Shared runtime regression: defaults, settings, cold restart, weather and views.

Run with the Python bundled with pebble-tool. Uses a disposable SDK profile;
all weather/health inputs are synthetic. Face-specific visual checks stay local.
"""
from pathlib import Path
import argparse
import json
import subprocess
from emulator import EmulatorSession


def common_settings(goal, weather=False):
    return {'TIME_MODE': 2, 'TEMP_UNIT': 0, 'STEP_GOAL': goal, 'WORLD': 0,
            'WEATHER_ENABLED': weather, 'DEMO_FALLBACK': False, 'LARGE_LABELS': False}


def run_smoke(project, default_goal=8000, saved_goal=12500, weather=True, detail=True):
    project = Path(project).resolve()
    with EmulatorSession(project) as emu:
        emu.install()
        emu.start()
        emu.logs.wait(r'Kit settings loaded: goal='+str(default_goal))
        emu.cli('emu-battery', '--percent', '76')
        emu.cli('emu-steps', '6842')
        emu.cli('emu-heart-rate', '64')
        emu.screenshot('fresh-default.png')
        emu.apply_clay(common_settings(saved_goal))
        mark = emu.restart()
        emu.logs.wait(r'Kit settings loaded: goal='+str(saved_goal), since=mark)
        emu.apply_clay(common_settings(default_goal))
        if weather:
            subprocess.run(['node', str(Path(__file__).with_name('emulator_weather_fixture.js')),
                            str(emu.output)], check=True)
            emu.weather_reply_once((emu.output/'weather.bin').read_bytes())
            mark = emu.logs.mark()
            emu.apply_settings(common_settings(default_goal, weather=True))
            emu.logs.wait('Weather snapshot accepted', since=mark)
            assert emu.weather_requests and emu.weather_replies == 1
            # Matching phone settings must survive READY on the next start.
            emu.apply_clay(common_settings(default_goal, weather=True))
            mark = emu.restart()
            emu.logs.wait('Weather cache restored', since=mark)
            assert emu.weather_replies == 1, 'Cache check was masked by another synthetic reply'
        emu.screenshot('face.png')
        if detail:
            mark = emu.logs.mark()
            emu.cli('emu-tap')
            emu.logs.wait('Frame detail', since=mark)
            mark = emu.logs.mark()
            emu.screenshot('detail.png')
            emu.logs.wait('Frame face', since=mark, timeout=12)
            emu.screenshot('timeout.png')
        emu.stop()
        report = emu.logs.report()
        assert not report['faults'], report['faults']
        result = {'uuid': str(emu.uuid), 'fresh_default_goal': default_goal,
                  'saved_goal_after_restart': saved_goal, 'synthetic_weather': weather,
                  'weather_reply_count': emu.weather_replies, 'report': report}
        (emu.output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
        print(project.name+': shared smoke passed (default, persisted goal, restart, cache, views and clean exits).', flush=True)
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('projects', type=Path, nargs='+')
    parser.add_argument('--default-goal', type=int, default=8000)
    parser.add_argument('--saved-goal', type=int, default=12500)
    parser.add_argument('--no-weather', action='store_true')
    parser.add_argument('--no-detail', action='store_true')
    args = parser.parse_args()
    for project in args.projects:
        run_smoke(project, args.default_goal, args.saved_goal, not args.no_weather, not args.no_detail)
