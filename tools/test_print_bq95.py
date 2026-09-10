#!/usr/bin/env python3
"""Send one short diagnostic receipt to the BQ95 already identified by probe_printer.

V5G command facts: https://github.com/Dejniel/TiMini-Print
This diagnostic deliberately sends one small job and never retries a print.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE_UUID = '-0000-1000-8000-00805f9b34fb'
# BQ95 maps to TiMini Print's V5G profile, whose A3 handler treats 04 as
# d2_status. Do not apply the older GB01 status bit mask to this camera.
# Reference: timiniprint/printing/runtime/v5g.py, _update_status.
V5G_ACCEPTED_STATES = {0x00, 0x04}


def packet(opcode, payload):
    checksum = 0
    for byte in payload:
        checksum ^= byte
        for _ in range(8):
            checksum = ((checksum << 1) ^ (7 if checksum & 128 else 0)) & 255
    return b'\x51\x78' + bytes([opcode, 0]) + len(payload).to_bytes(2, 'little') + payload + bytes([checksum, 255])


def prepare():
    from PIL import Image, ImageDraw, ImageFont
    im = Image.new('1', (384, 176), 1)
    draw = ImageDraw.Draw(im)
    font_path = '/System/Library/Fonts/Supplemental/Arial.ttf'
    title = ImageFont.truetype(font_path, 28)
    body = ImageFont.truetype(font_path, 21)
    draw.rectangle((12, 10, 371, 165), outline=0, width=2)
    for y, text, font in [
        (22, 'ОРАКУЛ НА СВЯЗИ', title),
        (69, 'Кот подключился к судьбе.', body),
        (102, 'BQ95 / тест печати', body),
        (133, '*  *  *', body),
    ]:
        draw.text((192, y), text, fill=0, font=font, anchor='mt')
    path = ROOT / 'docs/printer-test.png'
    im.save(path)
    parts = [packet(0xA3, b'\0'), packet(0xA4, b'\x33'),
             packet(0xA6, bytes.fromhex('aa551738445f5f5f44382c')),
             packet(0xAF, (10000).to_bytes(2, 'little')), packet(0xBE, b'\0'), packet(0xBD, b'\x0a')]
    pixels = im.load()
    for y in range(im.height):
        row = bytearray(48)
        for x in range(384):
            if not pixels[x, y]:
                row[x // 8] |= 1 << (x % 8)
        parts.append(packet(0xA2, row))
    parts.extend([packet(0xBD, b'\x19'), packet(0xA1, b'\x30\0'),
                  packet(0xA6, bytes.fromhex('aa55170000000000000017')), packet(0xA3, b'\0')])
    return path, b''.join(parts)


async def send(payload, report_stem='printer-test', metadata=None):
    from bleak import BleakClient
    probe = json.loads((ROOT / 'docs/printer-probe.json').read_text())
    assert probe['status'] == 'gatt_inspected' and len(probe['matches']) == 1
    target = probe['matches'][0]
    assert target['name'] == 'BQ95'
    report = {'timestamp': datetime.now(timezone.utc).isoformat(),
              'address': target['address'], 'notifications': [], 'bytes_sent': 0,
              'status': 'not_sent', 'last_device_status': None, 'physical_print_confirmed': False}
    if metadata: report.update(metadata)
    status_received = asyncio.Event()
    blocked = asyncio.Event()
    pending = bytearray()
    def notification(char, data):
        data = bytes(data)
        report['notifications'].append({'characteristic': char.uuid, 'hex': data.hex()})
        print('Notification:', data.hex(), flush=True)
        pending.extend(data)
        while pending:
            start = pending.find(b'\x51\x78')
            if start < 0:
                pending[:] = pending[-1:] if pending[-1:] == b'\x51' else b''
                break
            del pending[:start]
            if len(pending) < 6:
                break
            size = int.from_bytes(pending[4:6], 'little')
            if size > 64:
                del pending[0]
                continue
            if len(pending) < size + 8:
                break
            response = bytes(pending[:size + 8])
            body = response[6:-2]
            if response[-2:] != packet(response[2], body)[-2:]:
                del pending[0]
                continue
            del pending[:size + 8]
            if response[2] == 0xa3 and response[3] == 1 and body:
                report['last_device_status'] = body[0]
                if body[0] not in V5G_ACCEPTED_STATES: blocked.set()
                status_received.set()
    try:
        async with BleakClient(target['address'], timeout=15) as client:
            writer = client.services.get_characteristic('0000ae01' + BASE_UUID)
            assert writer is not None and 'write-without-response' in writer.properties
            for short in ['ae02', 'ae04']:
                await client.start_notify('0000' + short + BASE_UUID, notification)
            await asyncio.sleep(1)
            for attempt in range(3):
                await client.write_gatt_char(writer, packet(0xA3, b'\0'), response=False)
                try:
                    await asyncio.wait_for(status_received.wait(), timeout=3)
                    if blocked.is_set():
                        raise RuntimeError(f'Printer status 0x{report["last_device_status"]:02x}; no print data sent')
                    break
                except asyncio.TimeoutError:
                    if blocked.is_set() or attempt == 2:
                        raise RuntimeError('No valid state reply; no print data sent') from None
            report['status'] = 'sending'
            chunk_size = min(63, writer.max_write_without_response_size)
            for offset in range(0, len(payload), chunk_size):
                if blocked.is_set():
                    raise RuntimeError('Printer reported nonzero status; stopping without retry')
                chunk = payload[offset:offset + chunk_size]
                await client.write_gatt_char(writer, chunk, response=False)
                report['bytes_sent'] += len(chunk)
                await asyncio.sleep(0.012)
            await asyncio.sleep(3)
            report['status'] = 'sent' if not blocked.is_set() else 'printer_reported_error'
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        rendered = json.dumps(report, indent=2) + '\n'
        (ROOT / 'docs' / (report_stem+'-result.json')).write_text(rendered)
        with (ROOT / 'docs' / (report_stem+'-history.jsonl')).open('a') as history:
            history.write(json.dumps(report) + '\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--send', action='store_true', help='Physically print one test; otherwise only create the preview')
    args = parser.parse_args()
    assert packet(0xA3, b'\0') == bytes.fromhex('5178a30001000000ff')
    assert packet(0, b'123456789')[-2] == 0xf4
    preview, payload = prepare()
    print(f'Preview: {preview}; job: {len(payload)} bytes', flush=True)
    if args.send:
        try:
            asyncio.run(send(payload))
        except Exception as error:
            raise SystemExit(f'Test stopped: {type(error).__name__}: {error}') from None
