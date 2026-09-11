#!/usr/bin/env python3
"""Package a matching firmware and language-specific SD pack, with verified hashes."""
import argparse
from pathlib import Path
import hashlib
import json
import zipfile
from languages import ROOT, LANGUAGES, pack_root, firmware_env, deck_path
from prepare_pack import validate
from prepare_receipts import validate_receipt

def release_name(language):
    if language not in LANGUAGES: raise ValueError('Unsupported language')
    return 'Oracle-ADV-'+(ROOT/'VERSION').read_text().strip()+'-'+language

def package(language='en'):
    sd=pack_root(language)
    index=json.loads((sd/'ORACLE/index.json').read_text())
    if index.get('language')!=language: raise ValueError('SD pack language mismatch')
    cards=[json.loads((sd/'ORACLE/cards'/f'{ident}.json').read_text()) for ident in index['ids']]
    source=json.loads(deck_path(language).read_text())
    if [{k:v for k,v in c.items() if k!='schema'} for c in cards]!=source['cards']:
        raise ValueError('Stale SD pack: rebuild resources from the current deck')
    validate({'schema':1,'cards':cards},media_root=sd,require_media=True)
    firmware=ROOT/f'firmware/.pio/build/{firmware_env(language)}/firmware.bin'
    binary=firmware.read_bytes()
    if len(binary)>0x140000 or binary[0]!=0xe9 or int.from_bytes(binary[12:14],'little')!=9:
        raise ValueError('Missing, oversized or wrong-chip firmware')
    marker=('ORACLE_BUILD_LANGUAGE='+language).encode()
    if marker not in binary: raise ValueError('Firmware and resource languages do not match')
    version=release_name(language)
    files={version+'.bin':firmware,
           'README.md':ROOT/('docs/INSTALL.zh.md' if language=='zh' else 'docs/INSTALL.en.md' if language=='en' else 'docs/INSTALL.md'),
           'ORACLE/index.json':sd/'ORACLE/index.json',
           'ORACLE/config.example.json':sd/'ORACLE/config.example.json',
           'ORACLE/tls/roots.pem':sd/'ORACLE/tls/roots.pem'}
    for name in ('Hermes-LICENSE','NimBLE-Arduino-LICENSE','Apache-NimBLE-LICENSE','TinyCrypt-LICENSE'):
        files['third_party/'+name]=ROOT/'third_party'/name
    if language=='zh':
        from chinese_font import validate_book
        font=(sd/'ORACLE/fonts/chinese12.bin').read_bytes()
        validate_book(font)
        if font!=(ROOT/'sdcard/ORACLE/fonts/chinese12.bin').read_bytes():
            raise ValueError('Stale Chinese SD font')
        files['ORACLE/fonts/chinese12.bin']=sd/'ORACLE/fonts/chinese12.bin'
        files['third_party/Efont-LICENSE']=ROOT/'third_party/Efont-LICENSE'
    for c in cards:
        for rel in (f'ORACLE/cards/{c["id"]}.json',f'ORACLE/print/{c["id"]}.bin'):
            files[rel]=sd/rel
        validate_receipt(files[f'ORACLE/print/{c["id"]}.bin'].read_bytes(),c)
        for key in ('audio','image'):
            rel=c[key].lstrip('/'); files[rel]=sd/rel
        meta=json.loads((ROOT/'sdcard'/c['audio'].lstrip('/')).with_suffix('.json').read_text())
        if meta.get('text')!=c['text'] or meta.get('language','ru')!=language:
            raise ValueError('Stale or wrong-language voice: '+c['id'])
    checksums=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {name}\n' for name,p in sorted(files.items()))
    release=ROOT/'release'; release.mkdir(exist_ok=True)
    (release/(version+'.bin')).write_bytes(binary)
    archive=release/(version+'.zip'); temporary=archive.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as z:
        for name,p in sorted(files.items()): z.write(p,name)
        z.writestr('SHA256SUMS',checksums)
    with zipfile.ZipFile(temporary) as z:
        assert z.testzip() is None
        for line in z.read('SHA256SUMS').decode().splitlines():
            digest,name=line.split('  ',1); assert hashlib.sha256(z.read(name)).hexdigest()==digest
    temporary.replace(archive)
    print(f'{archive}\n{language}: {len(cards)} voiced cards, {archive.stat().st_size:,} bytes; archive verified')
    return archive

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--language',choices=LANGUAGES,default='en')
    package(parser.parse_args().language)
