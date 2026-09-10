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
    cases={'core':['core'],'theme':['core'],'print':['print_protocol'],'sound':['sound'],'volume':['sound']}
    for name,sources in cases.items():
        exe=out/name
        subprocess.run(flags+[str(ROOT/f'firmware/tests/{name}_test.cpp')]+
                       [str(ROOT/f'firmware/src/{s}.cpp') for s in sources]+['-o',str(exe)],check=True)
        subprocess.run([str(exe)],cwd=ROOT,check=True)
    print('Native tests passed. Tarot API tests additionally require ArduinoJson from PlatformIO.')

if __name__=='__main__': main()
