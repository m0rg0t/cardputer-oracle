#!/usr/bin/env python3
"""Validate a public release without requiring credentials or historical archives."""
from pathlib import Path
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import subprocess
import sys
import wave
import zipfile

from prepare_pack import ROOT, validate
from prepare_receipts import validate_receipt
from check_repo import known_secrets, issues
from languages import LANGUAGES, deck_path, pack_root, firmware_env
from package_demo import release_name


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main(language='en'):
    version=(ROOT/'VERSION').read_text().strip()
    sd=pack_root(language)
    cards=validate(json.loads(deck_path(language).read_text()),media_root=sd,require_media=True)
    index=json.loads((sd/'ORACLE/index.json').read_text())
    if index.get('language')!=language or index['ids']!=[c['id'] for c in cards]:
        raise ValueError('Wrong-language or stale catalog')
    counts=Counter(c['topic'] if c['topic'] in ('beer','tea') else 'humor_it' for c in cards)
    total_seconds=0
    for c in cards:
        with wave.open(str(sd/c['audio'].lstrip('/')),'rb') as wav:
            if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate())!=(1,2,16000):
                raise ValueError('Unexpected voice format: '+c['id'])
            total_seconds+=wav.getnframes()/wav.getframerate()
        validate_receipt((sd/'ORACLE/print'/f"{c['id']}.bin").read_bytes(),c)
    firmware=(ROOT/f'firmware/.pio/build/{firmware_env(language)}/firmware.bin').read_bytes()
    if firmware[0]!=0xe9 or len(firmware)>0x140000 or int.from_bytes(firmware[12:14],'little')!=9:
        raise ValueError('Wrong chip or oversized application image')
    if ('ORACLE_BUILD_LANGUAGE='+language).encode() not in firmware:
        raise ValueError('Wrong firmware language')
    archive_path=ROOT/f'release/{release_name(language)}.zip'
    secrets=known_secrets()
    if issues('firmware',firmware,secrets): raise ValueError('Credential detected in firmware')
    with zipfile.ZipFile(archive_path) as archive:
        names=archive.namelist()
        if archive.testzip() is not None: raise ValueError('Corrupt ZIP')
        if 'ORACLE/config.json' in names: raise ValueError('Private configuration in public ZIP')
        for name in names:
            if issues(name,archive.read(name),secrets): raise ValueError('Credential detected in public archive: '+name)
        for line in archive.read('SHA256SUMS').decode().splitlines():
            digest,name=line.split('  ',1)
            if sha(archive.read(name))!=digest: raise ValueError('Archive checksum mismatch: '+name)
        if archive.read(release_name(language)+'.bin')!=firmware: raise ValueError('Stale archive firmware')
        for name in names:
            if name.startswith('ORACLE/') and archive.read(name)!=(sd/name).read_bytes():
                raise ValueError('Stale resource: '+name)
    subprocess.run([sys.executable,str(ROOT/'tools/check_nimble_storage.py'),'--language',language],check=True)
    report={
        'version':version,'language':language,'validated_at_utc':datetime.now(timezone.utc).isoformat(),
        'firmware_bytes':len(firmware),'firmware_sha256':sha(firmware),'chip_id':9,
        'archive_sha256':sha(archive_path.read_bytes()),'archive_bytes':archive_path.stat().st_size,
        'archive_file_count':len(names),'archive_verified':True,'public_artifacts_secret_free':True,
        'cards':len(cards),'counts':dict(counts),'voices':len(cards),
        'images':len({c['image'] for c in cards}),'receipt_count':len(cards),
        'audio_seconds':round(total_seconds,4),
        'local_private_credentials_checked':bool(secrets),
        'physical_cardputer_verified':False,
        'ble_storage_check':json.loads((ROOT/f'docs/nimble-storage-{version}-{language}-validation.json').read_text()),
    }
    (ROOT/f'docs/release-{version}-{language}-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--language',choices=LANGUAGES,default='en')
    main(parser.parse_args().language)
