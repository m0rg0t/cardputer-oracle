#!/usr/bin/env python3
"""Build and run portable firmware tests without touching hardware or APIs."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    out=ROOT/'build/tests'; out.mkdir(parents=True,exist_ok=True)
    flags=[os.environ.get('CXX','c++'),'-std=c++17','-fno-exceptions','-Wall','-Wextra','-Werror',
           '-I'+str(ROOT/'firmware/include')]
    cases={'core':['core'],'theme':['core'],'print':['print_protocol'],'sound':['sound'],'volume':['sound'],'locale':['core']}
    cases['chinese']=['core','chinese_font']
    arduino_json=ROOT/'firmware/.pio/libdeps/oracle-adv/ArduinoJson/src'
    if arduino_json.is_dir():
        cases['tarot']=['core','tarot','tarot_api','chinese_font']
        flags+=['-I'+str(arduino_json)]
    for language in ('en','ru','zh'):
        for name,sources in cases.items():
            exe=out/(name+'-'+language)
            subprocess.run(flags+['-DORACLE_LANG_RU='+str(int(language=='ru')),'-DORACLE_LANG_ZH='+str(int(language=='zh')),str(ROOT/f'firmware/tests/{name}_test.cpp')]+
                           [str(ROOT/f'firmware/src/{s}.cpp') for s in sources]+['-o',str(exe)],check=True)
            subprocess.run([str(exe)],cwd=ROOT,check=True)
    print('Native tests passed for EN, RU and ZH.')
    if 'tarot' not in cases: print('SKIP: Tarot API tests require ArduinoJson from PlatformIO.')

if __name__=='__main__': main()
