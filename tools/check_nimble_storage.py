#!/usr/bin/env python3
"""Reject a firmware build whose BLE library still depends on NVS."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

from nimble_m5apps import ram_only_device
from languages import LANGUAGES, firmware_env

ROOT = Path(__file__).resolve().parents[1]


def verify(language='en'):
    environment=firmware_env(language)
    build = ROOT / 'firmware/.pio/build' / environment
    packages = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio')) / 'packages'
    nm = packages / 'toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-nm'
    libraries = list(build.glob('lib*/libNimBLE-Arduino.a'))
    assert len(libraries) == 1, 'Missing/ambiguous compiled NimBLE library'
    symbols = subprocess.check_output([str(nm), '-u', str(libraries[0])], text=True)
    unresolved = [line.split()[-1] for line in symbols.splitlines() if ' U ' in line]
    assert not any(s.startswith('nvs_') for s in unresolved), 'BLE still uses NVS'
    assert all(s in unresolved for s in ('esp_bt_controller_init', 'esp_bt_controller_enable', 'nimble_port_init')), 'BLE initialization was removed'
    printer = subprocess.check_output([str(nm), '-u', str(build / 'src/bq95_printer.cpp.o')], text=True)
    assert 'nvs_' not in printer, 'Printer still requires NVS'
    original = ROOT / 'firmware/.pio/libdeps' / environment / 'NimBLE-Arduino/src/NimBLEDevice.cpp'
    generated = build / 'oracle-generated/NimBLEDevice.cpp'
    assert generated.read_text() == ram_only_device(original.read_text()), 'Generated source is stale'
    stores = list(build.glob('lib*/NimBLE-Arduino/**/ble_store_nvs.c.o'))
    assert len(stores) == 1, 'Missing/ambiguous BLE persistent storage object'
    store_symbols = subprocess.check_output([str(nm), str(stores[0])], text=True)
    assert not store_symbols.strip(), 'Persistent BLE storage was compiled in'
    elf = build / 'firmware.elf'
    linked = subprocess.check_output([str(nm), '-C', str(elf)], text=True)
    assert 'NimBLEDevice::init(' in linked, 'BLE init is not linked'
    report = {
        'nimble_version': '1.4.3', 'ble_nvs_dependencies': [],
        'controller_and_host_init_preserved': True,
        'persistent_storage_object_empty': True,
        'downloaded_source_unchanged': True,
        'original_device_source_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
        'generated_device_source_sha256': hashlib.sha256(generated.read_bytes()).hexdigest(),
        'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
        'physical_ble_start_and_print_verified': False,
    }
    version=(ROOT/'VERSION').read_text().strip()
    report['language']=language
    (ROOT / f'docs/nimble-storage-{version}-{language}-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--language',choices=LANGUAGES,default='en')
    verify(parser.parse_args().language)
