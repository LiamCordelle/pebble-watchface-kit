from pathlib import Path
from types import SimpleNamespace
import importlib.util
import json
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('kit_emulator',ROOT/'tools/emulator.py')
emulator=importlib.util.module_from_spec(spec);spec.loader.exec_module(emulator)


class EmulatorUtilities(unittest.TestCase):
    def test_byte_cursor_and_log_report(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'log';path.write_text('Unicode snowman: ☃\n')
            logs=emulator.LogReader(path);mark=logs.mark()
            with path.open('a') as out:out.write('Frame face 10001ms\nFrame detail 4294966308ms\n')
            self.assertTrue(logs.read(mark).startswith('Frame face'))
            report=logs.report(mark)
            self.assertEqual(len(report['frames']),2)
            self.assertEqual(report['frames'][0]['wall_ms'],10001)
            self.assertEqual(len(report['clock_discontinuity_candidates']),1)
        report=emulator.parse_log('Heap Usage for App <Word Clock>: Used <99B> Still allocated <0B>\n'
                                  'Heap Usage for App <Quest: Used <99B> Still allocated <4B>\nAPP_FAULT x\n')
        self.assertEqual(report['exits'],[{'app':'Word Clock','allocated_bytes':0},{'app':'Quest','allocated_bytes':4}])
        self.assertEqual(len(report['faults']),1)

    def test_typed_transport(self):
        sdk=SimpleNamespace(Uint32=lambda value:('number',value),CString=lambda value:('text',value),ByteArray=lambda value:('bytes',value))
        keys={'A':1,'B':2,'C':3}
        self.assertEqual(emulator.encode_settings({'A':True,'B':'text','C':b'abc'},keys,sdk),
                         {1:('number',1),2:('text','text'),3:('bytes',b'abc')})
        self.assertEqual(emulator.encode_settings({'A':-1},keys,sdk)[1],('number',4294967295))
        with self.assertRaises(ValueError):emulator.encode_settings({'UNKNOWN':0},keys,sdk)
        with self.assertRaises(ValueError):emulator.encode_settings({'A':1.2},keys,sdk)

    def session(self, root):
        project=root/'project';project.mkdir()
        (project/'package.json').write_text(json.dumps({'pebble':{'displayName':'Example','uuid':'00000000-0000-0000-0000-000000000001'}}))
        session=emulator.EmulatorSession(project,root/'output')
        session.profile=root/'profile';session.profile.mkdir();(session.profile/'temporary').touch()
        session.backup=root/'backup'/'original';session.backup.mkdir(parents=True);(session.backup/'original').touch()
        session.recovery=root/'recovery.json';session.moved_original=True;session.owns_profile=True
        return session

    def test_restore_only_after_confirmed_stop(self):
        with tempfile.TemporaryDirectory() as temp:
            session=self.session(Path(temp))
            with patch.object(emulator,'emulator_running',return_value=False),patch.object(emulator.subprocess,'run'):
                session.close()
            self.assertTrue((session.profile/'original').exists())
            self.assertFalse(session.backup.parent.exists())
            self.assertTrue(json.loads(session.recovery.read_text())['restored'])

    def test_stuck_emulator_retains_both_profile_and_backup(self):
        with tempfile.TemporaryDirectory() as temp:
            session=self.session(Path(temp))
            with patch.object(emulator,'emulator_running',return_value=True),patch.object(emulator.subprocess,'run'),\
                 patch.object(emulator.time,'monotonic',side_effect=[0,9]):
                with self.assertRaisesRegex(RuntimeError,'backup retained'):session.close()
            self.assertTrue((session.profile/'temporary').exists())
            self.assertTrue((session.backup/'original').exists())

    def test_detection_errors_are_not_treated_as_no_emulator(self):
        with patch.object(emulator.subprocess,'run',return_value=SimpleNamespace(returncode=2)):
            with self.assertRaises(RuntimeError):emulator.emulator_running()

    def test_pending_recovery_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            record=Path(temp)/'recovery.json';record.write_text('{"restored":false,"backup":"original"}')
            with self.assertRaisesRegex(RuntimeError,'Unfinished'):emulator.ensure_no_pending_recovery(record)
            self.assertIn('original',record.read_text())
            record.write_text('{"restored":true}')
            emulator.ensure_no_pending_recovery(record)

    def test_restore_bookkeeping_failure_cannot_delete_original_on_retry(self):
        with tempfile.TemporaryDirectory() as temp:
            session=self.session(Path(temp))
            with patch.object(emulator,'emulator_running',return_value=False),patch.object(emulator.subprocess,'run'),\
                 patch.object(Path,'write_text',side_effect=OSError('record failed')):
                with self.assertRaises(OSError):session.close()
            self.assertTrue((session.profile/'original').exists())
            session.close()
            self.assertTrue((session.profile/'original').exists())


if __name__=='__main__':unittest.main()
