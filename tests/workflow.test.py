#!/usr/bin/env python3
"""Exercise dependency updates with real Git repos and release with a fake SDK."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
update = runpy.run_path(str(ROOT/'tools/update_release.py'))['update']
release = runpy.run_path(str(ROOT/'tools/release.py'))['release']


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='kit-workflow-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root/'source'
        self.source.mkdir()
        self.git(self.source, 'init', '-b', 'main')
        self.git(self.source, 'config', 'user.name', 'Test')
        self.git(self.source, 'config', 'user.email', 'test@example.invalid')
        (self.source/'VERSION').write_text('2.0.0\n')
        self.commit(self.source)
        self.project = self.root/'face'
        self.project.mkdir()
        self.git(self.project, 'init', '-b', 'main')
        self.git(self.project, 'config', 'user.name', 'Test')
        self.git(self.project, 'config', 'user.email', 'test@example.invalid')
        self.git(self.project, '-c', 'protocol.file.allow=always', 'submodule', 'add', '-b', 'main', str(self.source), 'shared-kit')
        self.commit(self.project)
        self.kit = self.project/'shared-kit'

    def git(self, root, *args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True, stderr=subprocess.PIPE).strip()

    def commit(self, root):
        self.git(root, 'add', '.')
        self.git(root, 'commit', '-m', 'Test snapshot')
        return self.git(root, 'rev-parse', 'HEAD')

    def test_latest_update_and_dirty_protection(self):
        old = self.git(self.kit, 'rev-parse', 'HEAD')
        (self.source/'VERSION').write_text('2.0.1\n')
        latest = self.commit(self.source)
        update(self.project)
        self.assertNotEqual(old, latest)
        self.assertEqual(self.git(self.kit, 'rev-parse', 'HEAD'), latest)
        (self.kit/'unfinished').write_text('Keep my changes')
        with self.assertRaisesRegex(RuntimeError, 'unfinished'):
            update(self.project)
        self.assertEqual((self.kit/'unfinished').read_text(), 'Keep my changes')

    def test_unpublished_commits_and_failed_fetch_are_preserved(self):
        self.git(self.kit, 'config', 'user.name', 'Test')
        self.git(self.kit, 'config', 'user.email', 'test@example.invalid')
        (self.kit/'local-fix').write_text('unpublished')
        local = self.commit(self.kit)
        with self.assertRaisesRegex(RuntimeError, 'outside origin/main'):
            update(self.project)
        self.assertEqual(self.git(self.kit, 'rev-parse', 'HEAD'), local)
        self.git(self.kit, 'remote', 'set-url', 'origin', str(self.root/'missing-remote'))
        with self.assertRaises(subprocess.CalledProcessError):
            update(self.project)
        self.assertEqual(self.git(self.kit, 'rev-parse', 'HEAD'), local)

    def test_release_forces_live_clean_build_and_records_exact_kit(self):
        package = {'name':'test-face','version':'1.0.0','pebble':{'uuid':'3bd45600-4ca0-49b6-8129-f840caaf0201'}}
        (self.project/'package.json').write_text(json.dumps(package))
        (self.project/'tests').mkdir()
        (self.project/'tests/run.py').write_text("from pathlib import Path\nPath('tests-passed').write_text('yes')\n")
        binaries = self.root/'bin'
        binaries.mkdir()
        sdk = binaries/'pebble'
        sdk.write_text('''#!/usr/bin/env python3
import json,os,sys,zipfile
from pathlib import Path
assert Path('tests-passed').is_file()
assert os.environ['LAB_SCENARIO']=='3'
assert os.environ['KIT_TOUCH_WINDOW_MS']=='0'
if sys.argv[1]=='clean':
    Path('cleaned').touch()
else:
    assert Path('cleaned').is_file()
    package=json.loads(Path('package.json').read_text())
    Path('build').mkdir(exist_ok=True)
    with zipfile.ZipFile('build/test-face.pbw','w') as out:
        out.writestr('appinfo.json',json.dumps({'uuid':package['pebble']['uuid'],'versionLabel':package['version']}))
''')
        sdk.chmod(0o755)
        self.commit(self.project)
        with patch.dict(os.environ, {'PATH':str(binaries)+os.pathsep+os.environ['PATH'],
                                     'LAB_SCENARIO':'0','KIT_TOUCH_WINDOW_MS':'10000'}):
            release(self.project)
        artifact = self.project/'build/test-face.pbw'
        receipt = json.loads(artifact.with_suffix('.provenance.json').read_text())
        self.assertEqual(receipt['kit_revision'], self.git(self.kit, 'rev-parse', 'HEAD'))
        self.assertEqual(receipt['pbw_sha256'], hashlib.sha256(artifact.read_bytes()).hexdigest())
        self.assertEqual(receipt['scenario'], 3)
        (self.kit/'unfinished').touch()
        with self.assertRaisesRegex(RuntimeError, 'Commit and share'):
            release(self.project)


if __name__ == '__main__':
    unittest.main()
