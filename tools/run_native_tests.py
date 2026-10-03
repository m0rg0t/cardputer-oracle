#!/usr/bin/env python3
"""Build and run portable firmware tests without touching hardware or APIs."""
import argparse
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--require-tarot', action='store_true',
                        help='fail unless ArduinoJson is available for Tarot tests')
    parser.add_argument('--arduino-json', type=Path,
                        default=ROOT/'firmware/.pio/libdeps/oracle-adv/ArduinoJson/src',
                        help='ArduinoJson source directory containing ArduinoJson.h')
    args = parser.parse_args(argv)
    has_tarot = (args.arduino_json/'ArduinoJson.h').is_file()
    if args.require_tarot and not has_tarot:
        parser.error('Tarot tests required, but ArduinoJson.h is missing from '
                     f'{args.arduino_json}; install the pinned firmware dependencies '
                     'or pass --arduino-json /path/to/ArduinoJson/src')
    flags = [os.environ.get('CXX', 'c++'), '-std=c++17', '-fno-exceptions',
             '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'firmware/include')]
    cases = {'core': ['core'], 'theme': ['core'], 'print': ['print_protocol'],
             'sound': ['sound'], 'volume': ['sound'], 'locale': ['core'],
             'chinese': ['core', 'chinese_font']}
    if has_tarot:
        cases['tarot'] = ['core', 'tarot', 'tarot_api', 'chinese_font']
        flags.append('-I'+str(args.arduino_json))
    else:
        print('SKIP: Tarot API tests require ArduinoJson; running the offline subset.', flush=True)
    count = 0
    with tempfile.TemporaryDirectory(prefix='oracle-native-tests-') as directory:
        out = Path(directory)
        for language in ('en', 'ru', 'zh'):
            for name, sources in cases.items():
                exe = out/(name+'-'+language)
                subprocess.run(flags + [
                    '-DORACLE_LANG_RU='+str(int(language == 'ru')),
                    '-DORACLE_LANG_ZH='+str(int(language == 'zh')),
                    str(ROOT/f'firmware/tests/{name}_test.cpp'),
                ] + [str(ROOT/f'firmware/src/{source}.cpp') for source in sources]
                  + ['-o', str(exe)], check=True)
                subprocess.run([str(exe)], cwd=ROOT, check=True)
                count += 1
    coverage = 'including Tarot' if has_tarot else 'offline subset; Tarot skipped'
    print(f'{count} native test programs passed across EN, RU and ZH ({coverage}).')


if __name__ == '__main__':
    main()

