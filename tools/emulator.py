"""Reusable QEMU test support. Run SDK-dependent sessions with pebble-tool Python.

Consumers own scenarios, settings meanings, artwork and pixel assertions.
Importing this module does not boot QEMU or require libpebble2.
"""
from pathlib import Path
from types import SimpleNamespace
from uuid import UUID
import fcntl
import json
import os
import re
import shutil
import subprocess
import tempfile
import time


class LogReader:
    def __init__(self, path):
        self.path = Path(path)

    def mark(self):
        return self.path.stat().st_size if self.path.exists() else 0

    def read(self, since=0):
        if not self.path.exists():
            return ''
        with self.path.open('rb') as source:
            source.seek(since)
            return source.read().decode('utf-8', errors='replace')

    def wait(self, pattern, since=0, timeout=12):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = self.read(since)
            if re.search(pattern, text):
                return text
            time.sleep(.2)
        raise RuntimeError('Expected emulator log was not observed: ' + pattern)

    def report(self, since=0):
        return parse_log(self.read(since))


def parse_log(text):
    frames = [{'view': view, 'wall_ms': int(ms)}
              for view, ms in re.findall(r'Frame (\w+) (\d+)ms', text)]
    exits = [{'app': app, 'allocated_bytes': int(size)} for app, size in re.findall(
        r'Heap Usage for App <([^>:\n]+)>?:[^\n]*Still allocated <(\d+)B>', text)]
    return {
        'frames': frames,
        # Retain every sample. A wrap-sized value is a candidate, not proof of
        # a clock step and not grounds for silently discarding a slow frame.
        'clock_discontinuity_candidates': [frame for frame in frames if frame['wall_ms'] >= 2**31],
        'faults': re.findall(r'^.*(?:APP_FAULT|stack overflow|app crash|JS failed).*$', text, re.I | re.M),
        'starts': re.findall(r'Watchface kit (\S+) started', text),
        'exits': exits,
        'weather_acceptances': text.count('Weather snapshot accepted'),
    }


def emulator_running():
    result = subprocess.run(['pgrep', '-f', r'qemu-(system|pebble)'], capture_output=True)
    if result.returncode not in (0, 1):
        raise RuntimeError('Cannot inspect emulator processes; leave SDK profiles untouched.')
    return result.returncode == 0


def ensure_no_pending_recovery(path):
    if path.exists():
        try: state = json.loads(path.read_text())
        except (ValueError, OSError) as error:
            raise RuntimeError('Cannot inspect prior profile recovery record: '+str(path)) from error
        if not state.get('restored'):
            raise RuntimeError('Unfinished emulator profile recovery; preserve backup and inspect '+str(path))


def _sdk():
    from libpebble2.communication import PebbleConnection
    from libpebble2.protocol.apps import AppRunState, AppRunStateRequest, AppRunStateStart, AppRunStateStop
    from libpebble2.communication.transports.websocket import MessageTargetPhone
    from libpebble2.communication.transports.websocket.protocol import (
        AppConfigSetup, AppConfigResponse, WebSocketPhonesimAppConfig, WebSocketPhonesimConfigResponse)
    from libpebble2.services.appmessage import AppMessageService, Uint32, CString, ByteArray
    from pebble_tool.sdk.emulator import ManagedEmulatorTransport
    from pebble_tool.sdk import get_sdk_persist_dir
    return SimpleNamespace(**locals())


def encode_settings(values, keys, sdk):
    payload = {}
    for key, value in values.items():
        if key not in keys:
            raise ValueError('Unknown compiled AppMessage key: ' + key)
        if isinstance(value, (bool, int)):
            if not -(2**31) <= value < 2**32:
                raise ValueError('AppMessage integer out of range: ' + key)
            encoded = sdk.Uint32(int(value) & 0xffffffff)
        elif isinstance(value, str):
            encoded = sdk.CString(value)
        elif isinstance(value, (bytes, bytearray)):
            encoded = sdk.ByteArray(bytes(value))
        else:
            raise ValueError('Unsupported AppMessage value: ' + key)
        payload[keys[key]] = encoded
    return payload


class EmulatorSession:
    def __init__(self, project, output=None, platform='emery'):
        self.project = Path(project).resolve()
        self.output = Path(output or self.project/'shots/emulator-smoke').resolve()
        package = json.loads((self.project/'package.json').read_text())
        self.package_name = package.get('name', self.project.name)
        self.name = package['pebble']['displayName']
        self.uuid = UUID(package['pebble']['uuid'])
        self.platform = platform
        self.connection = self.service = self.log_process = self.lock = None
        self.backup = self.profile = None
        self.moved_original = self.owns_profile = False
        self.weather_requests = []
        self.weather_replies = 0

    def __enter__(self):
        self.output.mkdir(parents=True, exist_ok=True)
        self.recovery = self.output/'profile-recovery.json'
        ensure_no_pending_recovery(self.recovery)
        self.sdk = _sdk()
        self.lock = open(Path(tempfile.gettempdir())/'pebble-watchface-kit-emulator.lock', 'a')
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            if emulator_running():
                raise RuntimeError('Another emulator is active; leave it untouched.')
            self.profile = Path(self.sdk.get_sdk_persist_dir(self.platform))
            self.backup = Path(tempfile.mkdtemp(prefix='kit-emulator-profile-'))/'original'
            self.recovery.write_text(json.dumps({'profile': str(self.profile), 'backup': str(self.backup),
                                                 'pid': os.getpid(), 'restored': False}, indent=2)+'\n')
            if self.profile.exists():
                shutil.move(str(self.profile), str(self.backup))
                self.moved_original = True
            self.owns_profile = True
            self.log_file = (self.output/'emulator.log').open('w')
            self.logs = LogReader(self.output/'emulator.log')
            self.log_process = subprocess.Popen(['pebble', 'logs', '--emulator', self.platform],
                cwd=self.project, stdout=self.log_file, stderr=subprocess.STDOUT,
                env=dict(os.environ, PYTHONUNBUFFERED='1'))
            # Cold PebbleOS boot takes ~18 seconds on the supported local SDK.
            # Connection attempts are bounded after that boot allowance.
            time.sleep(18)
            for attempt in range(3):
                connection = self.sdk.PebbleConnection(self.sdk.ManagedEmulatorTransport(self.platform))
                try:
                    connection.connect()
                    self.connection = connection
                    break
                except Exception:
                    try: connection.transport.ws.close()
                    except Exception: pass
                    if attempt == 2:
                        raise
                    time.sleep(2)
            self.connection.run_async()
            self.service = self.sdk.AppMessageService(self.connection)
            # Avoid the transaction-ID range used by the SDK phone simulator.
            self.service._current_txid = 180
            self.keys = json.loads((self.project/'build/js/message_keys.json').read_text())
            return self
        except BaseException:
            self.close()
            raise

    def cli(self, *args):
        result = subprocess.run(['pebble', *map(str, args), '--emulator', self.platform],
            cwd=self.project, capture_output=True, text=True, check=True, timeout=60)
        with (self.output/'commands.log').open('a') as output:
            output.write(' '.join(map(str, args))+'\n'+result.stdout+result.stderr+'\n')
        if args and str(args[0]).startswith('emu-'):
            time.sleep(.8)  # service callbacks/deferred painting follow CLI ACK
        return result

    def install(self, pbw=None):
        self.cli('install', pbw or self.project/'build'/(self.package_name+'.pbw'))

    def assert_active(self):
        current = self.connection.send_and_read(
            self.sdk.AppRunState(data=self.sdk.AppRunStateRequest()), self.sdk.AppRunState)
        if current.data.uuid != self.uuid:
            raise RuntimeError('Unexpected active app UUID: ' + str(current.data.uuid))

    def start(self):
        self.connection.send_packet(self.sdk.AppRunState(data=self.sdk.AppRunStateStart(uuid=self.uuid)))
        time.sleep(1)
        self.assert_active()

    def stop(self):
        mark = self.logs.mark()
        self.connection.send_packet(self.sdk.AppRunState(data=self.sdk.AppRunStateStop(uuid=self.uuid)))
        pattern = r'Heap Usage for App <'+re.escape(self.name)+r'>?:[^\n]*Still allocated <0B>'
        self.logs.wait(pattern, since=mark)

    def restart(self):
        self.stop()
        mark = self.logs.mark()
        self.start()
        return mark

    def apply_settings(self, values):
        self.service.send_message(self.uuid, encode_settings(values, self.keys, self.sdk))
        time.sleep(.8)

    def apply_clay(self, values):
        self.connection.transport.send_packet(self.sdk.WebSocketPhonesimAppConfig(
            config=self.sdk.AppConfigSetup()), target=self.sdk.MessageTargetPhone())
        self.connection.read_transport_message(self.sdk.MessageTargetPhone,
            self.sdk.WebSocketPhonesimConfigResponse, timeout=10)
        self.connection.transport.send_packet(self.sdk.WebSocketPhonesimAppConfig(
            config=self.sdk.AppConfigResponse(data=json.dumps(values))), target=self.sdk.MessageTargetPhone())
        time.sleep(.8)

    def screenshot(self, name):
        path = self.output/name
        self.cli('screenshot', '--no-open', path)
        return path

    def weather_reply_once(self, encoded):
        def incoming(transaction, app, data):
            key = self.keys['REQUEST_WEATHER']
            if app != self.uuid or key not in data:
                return
            self.weather_requests.append(data[key])
            if self.weather_replies:
                return
            self.service.send_message(self.uuid, {self.keys['WEATHER_ID']: self.sdk.Uint32(data[key]),
                                                  self.keys['WEATHER']: self.sdk.ByteArray(encoded)})
            self.weather_replies += 1
        self.service.register_handler('appmessage', incoming)

    def close(self):
        errors = []
        try:
            if self.service:
                try: self.service.shutdown()
                except Exception as error: errors.append(str(error))
                self.service = None
            if self.connection:
                try: self.connection.transport.ws.close()
                except Exception as error: errors.append(str(error))
                self.connection = None
            if self.log_process:
                try:
                    self.log_process.terminate()
                    try: self.log_process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        self.log_process.kill()
                        self.log_process.wait()
                except Exception as error:
                    errors.append(str(error))
                finally:
                    try: self.log_file.close()
                    except Exception as error: errors.append(str(error))
                    self.log_process = None
            if self.owns_profile:
                try: subprocess.run(['pebble', 'kill'], capture_output=True, timeout=20)
                except Exception as error: errors.append(str(error))
                deadline = time.monotonic()+8
                while emulator_running() and time.monotonic() < deadline:
                    time.sleep(.2)
                if emulator_running():
                    raise RuntimeError('Owned QEMU did not stop. Profile backup retained at '+str(self.backup))
                if self.profile.exists():
                    shutil.rmtree(self.profile)
                if self.moved_original:
                    shutil.move(str(self.backup), str(self.profile))
                    self.moved_original = False
                # No retry may remove the restored original, even if the
                # following recovery bookkeeping fails.
                self.owns_profile = False
                self.backup.parent.rmdir()
                self.recovery.write_text(json.dumps({'profile': str(self.profile), 'restored': True}, indent=2)+'\n')
            if errors:
                raise RuntimeError('Emulator cleanup reported: '+'; '.join(errors))
        finally:
            if self.lock:
                self.lock.close()
                self.lock = None

    def __exit__(self, kind, error, traceback):
        self.close()
