#!/usr/bin/env python3
"""Validate the firmware-encoded tea receipt stream and optionally send one copy."""
import argparse
import asyncio
import json
import hashlib
from prepare_receipts import ROOT, ART_SIZE, validate_receipt
from test_print_bq95 import packet, send

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--send',action='store_true')
    args=parser.parse_args()
    card=json.loads((ROOT/'sdcard/ORACLE/cards/tea_ceremony.json').read_text())
    receipt=(ROOT/'sdcard/ORACLE/print/tea_ceremony.bin').read_bytes()
    height=validate_receipt(receipt,card)
    parts=[packet(0xa3,b'\0'),packet(0xa4,b'\x33'),packet(0xa6,bytes.fromhex('aa551738445f5f5f44382c')),
           packet(0xaf,(10000).to_bytes(2,'little')),packet(0xbe,b'\0'),packet(0xbd,b'\x0a')]
    parts.extend(packet(0xa2,receipt[20+y*48:20+(y+1)*48]) for y in range(height))
    parts.extend([packet(0xbd,b'\x19'),packet(0xa1,b'\x30\0'),packet(0xa6,bytes.fromhex('aa55170000000000000017')),packet(0xa3,b'\0')])
    wire=(ROOT/'docs/prediction-test-wire.bin').read_bytes()
    assert wire==b''.join(parts), 'Firmware stream does not match the verified BQ95 protocol'
    print(f'Validated C++ firmware stream: {height} rows, {len(wire)} bytes',flush=True)
    if args.send: asyncio.run(send(wire,report_stem='prediction-print',metadata={
        'card_id':card['id'],'illustration_width':ART_SIZE,'receipt_sha256':hashlib.sha256(receipt).hexdigest(),
        'firmware_stream_sha256':hashlib.sha256(wire).hexdigest()}))

if __name__=='__main__':main()
