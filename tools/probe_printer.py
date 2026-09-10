#!/usr/bin/env python3
"""Discover a named BLE printer and optionally enumerate GATT, without printing."""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path


async def probe(args):
    from bleak import BleakClient, BleakScanner

    found = await BleakScanner.discover(timeout=args.timeout, return_adv=True)
    matches = []
    devices = []
    for device, advertisement in found.values():
        name = advertisement.local_name or device.name or ""
        if name.casefold() != args.name.casefold():
            continue
        if args.address and device.address.casefold() != args.address.casefold():
            continue
        devices.append(device)
        matches.append({
            "name": name,
            "address": device.address,
            "rssi": advertisement.rssi,
            "advertised_services": advertisement.service_uuids,
            "manufacturer_data": {str(k): v.hex() for k, v in advertisement.manufacturer_data.items()},
        })
    report = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "target_name": args.name,
        "advertisement_count": len(found),
        "matches": matches,
        "status": "discovered" if matches else "not_found",
        "print_data_sent": False,
    }
    if args.inspect and len(devices) == 1:
        try:
            async with BleakClient(devices[0], timeout=15) as client:
                matches[0]["gatt_services"] = [
                    {"uuid": service.uuid, "characteristics": [
                        {"uuid": char.uuid, "handle": char.handle, "properties": char.properties,
                         "max_write_without_response_size": char.max_write_without_response_size}
                        for char in service.characteristics
                    ]}
                    for service in client.services
                ]
                report["status"] = "gatt_inspected"
        except Exception as error:
            report["status"] = "connection_failed"
            report["error"] = f"{type(error).__name__}: {error}"
    elif args.inspect and len(devices) > 1:
        report["status"] = "ambiguous_use_address"
    rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    print(rendered, end="")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    return 0 if report["status"] in ("discovered", "gatt_inspected") else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="BQ95", help="Exact advertised printer name")
    parser.add_argument("--address", help="Select one device if several advertise the same name")
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--inspect", action="store_true", help="Connect only to one unambiguous match and enumerate services")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 60:
        parser.error("timeout must be between 1 and 60 seconds")
    try:
        return asyncio.run(probe(args))
    except (ImportError, OSError, RuntimeError) as error:
        parser.exit(1, f"{type(error).__name__}: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
