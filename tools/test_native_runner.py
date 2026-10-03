"""Runner coverage checks; mocked subprocesses do not replace firmware tests."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import run_native_tests as runner


class NativeRunnerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.headers = Path(self.directory.name)

    def test_required_tarot_fails_before_compiling_when_header_is_missing(self):
        with patch.object(runner.subprocess, 'run') as run, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                runner.main(['--require-tarot', '--arduino-json', str(self.headers)])
        self.assertEqual(error.exception.code, 2)
        run.assert_not_called()

    def test_subset_explicitly_reports_skip_and_21_programs(self):
        output = io.StringIO()
        with patch.object(runner.subprocess, 'run') as run, contextlib.redirect_stdout(output):
            runner.main(['--arduino-json', str(self.headers)])
        self.assertEqual(run.call_count, 42)
        self.assertIn('SKIP: Tarot', output.getvalue())
        self.assertIn('21 native test programs', output.getvalue())

    def test_required_tarot_covers_all_three_languages(self):
        (self.headers/'ArduinoJson.h').write_text('// synthetic fixture\n')
        output = io.StringIO()
        with patch.object(runner.subprocess, 'run') as run, contextlib.redirect_stdout(output):
            runner.main(['--require-tarot', '--arduino-json', str(self.headers)])
        self.assertEqual(run.call_count, 48)
        commands = [call.args[0] for call in run.call_args_list]
        tarot = [cmd for cmd in commands if any(str(arg).endswith('/tarot_test.cpp') for arg in cmd)]
        self.assertEqual(len(tarot), 3)
        self.assertEqual({tuple(flag for flag in cmd if flag.startswith('-DORACLE_LANG_'))
                          for cmd in tarot}, {
            ('-DORACLE_LANG_RU=0', '-DORACLE_LANG_ZH=0'),
            ('-DORACLE_LANG_RU=1', '-DORACLE_LANG_ZH=0'),
            ('-DORACLE_LANG_RU=0', '-DORACLE_LANG_ZH=1'),
        })
        self.assertIn('24 native test programs', output.getvalue())
        self.assertIn('including Tarot', output.getvalue())


if __name__ == '__main__':
    unittest.main()
