#!/usr/bin/env python3
"""Audit exact indexed Git blobs; report paths/rules only, never secret values."""
from pathlib import Path
import argparse
import json
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PATTERNS=[('API token', re.compile(rb'\bsk[-_][A-Za-z0-9_-]{16,}')),
          ('GitHub token', re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})')),
          ('private key', re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----')),
          ('AWS access key', re.compile(rb'\bAKIA[A-Z0-9]{16}\b'))]

def known_secrets():
    path=ROOT/'release/private/config.json'
    if not path.is_file(): return []
    data=json.loads(path.read_text())
    values=[data.get('tarot',{}).get('token','')]+[n.get('password','') for n in data.get('wifi',[])]
    return [s.encode() for s in values if len(s)>=8 and not s.startswith('YOUR_')]

def issues(path,data,secrets):
    found=[]
    if any(s in data for s in secrets): found.append((path,'known private credential'))
    # Restrict heuristic patterns to text; exact private values are checked in all blobs.
    try: data.decode('utf-8')
    except UnicodeDecodeError: return found
    for label,pattern in PATTERNS:
        if pattern.search(data): found.append((path,label))
    return found

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site',action='store_true',help='Also check the built public website')
    args=parser.parse_args()
    ignored=subprocess.check_output(['git','ls-files','--cached','--ignored','--exclude-standard','-z'],cwd=ROOT)
    failures=[(p.decode(),'ignored path staged') for p in ignored.split(b'\0') if p]
    entries=subprocess.check_output(['git','ls-files','--stage','-z'],cwd=ROOT).split(b'\0')
    records=[]
    for entry in entries:
        if not entry: continue
        info,name=entry.split(b'\t',1); mode,oid,stage=info.split()
        name=name.decode()
        if mode!=b'100644' and mode!=b'100755': failures.append((name,'non-regular indexed file'))
        if stage!=b'0': failures.append((name,'unmerged index'))
        records.append((name,oid))
    if not records: raise SystemExit('Nothing indexed; stage the intended public files first.')
    proc=subprocess.run(['git','cat-file','--batch'],input=b'\n'.join(oid for _,oid in records)+b'\n',
                        stdout=subprocess.PIPE,cwd=ROOT,check=True)
    raw=proc.stdout; pos=0; secrets=known_secrets()
    for name,_ in records:
        end=raw.index(b'\n',pos); header=raw[pos:end].split(); size=int(header[2]);pos=end+1
        data=raw[pos:pos+size];pos+=size+1
        if size>=95*1024*1024: failures.append((name,'oversized Git blob'))
        failures.extend(issues(name,data,secrets))
    if args.site:
        for p in (ROOT/'build/site').rglob('*'):
            if p.is_file(): failures.extend(issues(str(p.relative_to(ROOT)),p.read_bytes(),secrets))
    if failures:
        for name,rule in failures: print(f'BLOCKED: {name}: {rule}')
        raise SystemExit(1)
    print(f'PASS: {len(records)} indexed files; no ignored paths, oversized blobs or detected credentials')

if __name__=='__main__': main()
