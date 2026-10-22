#!/usr/bin/env python3
"""Run deterministic kit tests without an emulator or network."""
import ctypes
import datetime as dt
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/tests'
OUT.mkdir(parents=True,exist_ok=True)
def run(args): subprocess.run(args,cwd=ROOT,check=True)
run(['node','tests/phone.test.js'])
run(['node','tests/config.test.js'])
run(['node','tests/bootstrap.test.js'])
run(['node','tests/emulator.test.js'])
run(['python3','tests/emulator.test.py'])
run(['cc','-std=c11','-Wall','-Wextra','-Werror','tests/calc.test.c',
     'src/c/kit/kit_calc.c','-o',str(OUT/'calc-tests')])
run([str(OUT/'calc-tests')])
run(['cc','-std=c11','-D_DARWIN_C_SOURCE','-Dtime=kit_test_time','-Itests/data-stubs',
     '-Wall','-Wextra','-Werror','tests/data.test.c','src/c/kit/kit_data.c',
     'src/c/kit/kit_calc.c','src/c/kit/kit_core.c','-o',str(OUT/'data-tests')])
run([str(OUT/'data-tests')])
run(['cc','-std=c11','-Itests/settings-stubs','-Wall','-Wextra','-Werror',
     'tests/settings.test.c','src/c/kit/kit_settings.c','src/c/kit/kit_core.c',
     '-o',str(OUT/'settings-tests')])
run([str(OUT/'settings-tests')])
run(['cc','-std=c11','-Wall','-Wextra','-Werror','tests/core.test.c','src/c/kit/kit_core.c','-o',str(OUT/'core-tests')])
run([str(OUT/'core-tests'),str(OUT/'weather.bin')])
run(['cc','-std=c11','-D_DARWIN_C_SOURCE','-DPBL_HEALTH',
     '-Dtime=health_test_time','-Itests/health-stubs','-Wall','-Wextra','-Werror',
     '-Wno-unused-parameter','tests/health.test.c','src/c/kit/kit_health.c',
     'src/c/kit/kit_hr_history.c','src/c/kit/kit_core.c','-o',str(OUT/'health-tests')])
run([str(OUT/'health-tests')])
run(['cc','-std=c11','-D_DARWIN_C_SOURCE','-DPBL_HEALTH',
     '-Itests/health-stubs','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
     'tests/history.test.c','src/c/kit/kit_hr_history.c','src/c/kit/kit_core.c',
     '-o',str(OUT/'history-tests')])
run([str(OUT/'history-tests')])
libpath=OUT/'libkit.dylib'
run(['cc','-std=c11','-shared','-fPIC','src/c/kit/kit_core.c','-o',str(libpath)])
lib=ctypes.CDLL(str(libpath))
lib.kit_world_offset.argtypes=[ctypes.c_int,ctypes.c_int32]
lib.kit_world_offset.restype=ctypes.c_int
zones=['Pacific/Auckland','UTC','Europe/London','America/New_York','America/Los_Angeles','Asia/Tokyo','Australia/Sydney']
checks=0
for year in range(2024,2038):
    stamp=int(dt.datetime(year,1,1,tzinfo=dt.timezone.utc).timestamp())
    end=int(dt.datetime(year+1,1,1,tzinfo=dt.timezone.utc).timestamp())
    while stamp<end:
        utc=dt.datetime.fromtimestamp(stamp,dt.timezone.utc)
        for index,name in enumerate(zones):
            expected=int(utc.astimezone(ZoneInfo(name)).utcoffset().total_seconds()/60)
            actual=lib.kit_world_offset(index,stamp)
            assert actual==expected,(name,utc,actual,expected)
            checks+=1
        stamp+=6*3600
print(f'Time zones passed {checks:,} independent zoneinfo comparisons (2024–2037).')
run(['python3','tests/workflow.test.py'])
