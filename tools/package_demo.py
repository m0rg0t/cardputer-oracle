#!/usr/bin/env python3
"""Package only the active validated SD catalog, application image and installation notes."""
from pathlib import Path
import hashlib
import json
import zipfile
from prepare_pack import ROOT, validate
from prepare_receipts import validate_receipt

sd=ROOT/'sdcard'
index=json.loads((sd/'ORACLE/index.json').read_text())
cards=[json.loads((sd/'ORACLE/cards'/f'{ident}.json').read_text()) for ident in index['ids']]
validate({'schema':1,'cards':cards},require_media=True)
firmware=ROOT/'firmware/.pio/build/oracle-adv/firmware.bin'
if not firmware.is_file() or firmware.stat().st_size>0x140000: raise SystemExit('Missing/oversized firmware')
version='Oracle-ADV-'+(ROOT/'VERSION').read_text().strip()+'-demo'
files={version+'.bin':firmware,'README.md':ROOT/'docs/INSTALL.md','DEVICE_CHECK.md':ROOT/'docs/DEVICE_CHECK.md','ORACLE/index.json':sd/'ORACLE/index.json','third_party/Hermes-LICENSE':ROOT/'third_party/Hermes-LICENSE'}
files['ORACLE/config.example.json']=sd/'ORACLE/config.example.json'
files['ORACLE/tls/roots.pem']=sd/'ORACLE/tls/roots.pem'
files['TAROT.md']=ROOT/'docs/TAROT.md'
for name in ('NimBLE-Arduino-LICENSE','Apache-NimBLE-LICENSE','TinyCrypt-LICENSE'):
    files['third_party/'+name]=ROOT/'third_party'/name
for c in cards:
    rel=f'ORACLE/cards/{c["id"]}.json';files[rel]=sd/rel
    rel=f'ORACLE/print/{c["id"]}.bin'; files[rel]=sd/rel
    validate_receipt(files[rel].read_bytes(),c)
    for k in ('audio','image'):
        rel=c[k].lstrip('/');files[rel]=sd/rel
checksums=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {name}\n' for name,p in sorted(files.items()))
release=ROOT/'release';release.mkdir(exist_ok=True)
(release/(version+'.bin')).write_bytes(firmware.read_bytes())
archive=release/(version+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for name,p in sorted(files.items()):z.write(p,name)
    z.writestr('SHA256SUMS',checksums)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for line in z.read('SHA256SUMS').decode().splitlines():
        digest,name=line.split('  ',1);assert hashlib.sha256(z.read(name)).hexdigest()==digest
print(f'{archive}\n{len(cards)} voiced cards, {archive.stat().st_size:,} bytes; all archive hashes verified')
