#!/usr/bin/env python3
"""Build a matching firmware + SD resource release. No API calls or secrets needed."""
import argparse
import shutil
import subprocess
from languages import ROOT, LANGUAGES, firmware_env
from prepare_pack import build as build_pack
from prepare_receipts import build as build_receipts
from package_demo import package

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--language',choices=LANGUAGES,default='en')
    parser.add_argument('--resources-only',action='store_true',help='Build the selected SD resources without compiling firmware')
    args=parser.parse_args()
    if args.language=='zh':
        from chinese_font import build as build_font
        build_font()
    build_pack(language=args.language,strict=True)
    build_receipts(args.language)
    if args.resources_only: return
    pio=shutil.which('pio') or shutil.which('platformio')
    if not pio: raise SystemExit('Install PlatformIO, then run this command again.')
    subprocess.run([pio,'run','-e',firmware_env(args.language)],cwd=ROOT/'firmware',check=True)
    package(args.language)

if __name__=='__main__': main()
