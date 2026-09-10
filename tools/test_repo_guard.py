"""Exercise the publication guard against staged secrets and ignored files."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class RepositoryGuardTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'build').mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='guard-',dir=ROOT/'build')
        self.root=Path(self.temp.name)
        (self.root/'tools').mkdir()
        shutil.copy2(ROOT/'tools/check_repo.py',self.root/'tools/check_repo.py')
        self.git('init','-q','-b','main')
    def tearDown(self): self.temp.cleanup()
    def git(self,*args):
        return subprocess.run(['git',*args],cwd=self.root,check=True,capture_output=True)
    def check(self,*args):
        return subprocess.run([sys.executable,'tools/check_repo.py',*args],cwd=self.root,capture_output=True,text=True)
    def test_public_example_passes(self):
        (self.root/'config.example.json').write_text('{"token":"YOUR_LITELLM_VIRTUAL_KEY"}')
        self.git('add','config.example.json')
        self.assertEqual(self.check().returncode,0)
    def test_checks_index_not_clean_worktree(self):
        token='sk'+'-'+'A'*24
        p=self.root/'accidental.txt';p.write_text(token);self.git('add',p.name)
        p.write_text('clean working tree')
        result=self.check()
        self.assertNotEqual(result.returncode,0)
        self.assertNotIn(token,result.stdout+result.stderr)
    def test_force_added_ignored_file_is_blocked(self):
        (self.root/'.gitignore').write_text('.env\n')
        (self.root/'.env').write_text('NOT_A_SECRET=example')
        self.git('add','-f','.env')
        self.assertIn('ignored path staged',self.check().stdout)
    def test_public_site_is_checked(self):
        (self.root/'README.md').write_text('safe');self.git('add','README.md')
        site=self.root/'build/site';site.mkdir(parents=True)
        (site/'index.html').write_text('sk'+'-'+'B'*24)
        self.assertNotEqual(self.check('--site').returncode,0)

if __name__=='__main__': unittest.main()
