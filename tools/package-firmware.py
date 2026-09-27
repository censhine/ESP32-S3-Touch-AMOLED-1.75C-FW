#!/usr/bin/env python3
"""Create a verified, addressed candidate bundle. Never accesses a device."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / "firmware" / "build"
    destination = args.destination.resolve()
    if destination.exists():
        parser.error("destination already exists; use a new directory")
    config = json.loads((build / "flasher_args.json").read_text())
    if config["flash_settings"] != {"flash_mode": "dio", "flash_size": "32MB", "flash_freq": "80m"}:
        parser.error("unexpected target flash configuration")

    # Addresses and upper bounds are intentional for this C-only layout.
    expected = {
        0x000000: ("bootloader/bootloader.bin", 0x8000),
        0x008000: ("partition_table/partition-table.bin", 0x1000),
        0x10D000: ("ota_data_initial.bin", 0x2000),
        0x110000: ("srmodels/srmodels.bin", 0xF0000),
        0x200000: ("round-wing-1_75c-brookesia.bin", 0x800000),
        0xA00000: ("storage.bin", 0x600000),
    }
    actual = {int(offset, 0): filename for offset, filename in config["flash_files"].items()}
    if actual != {offset: spec[0] for offset, spec in expected.items()}:
        parser.error("flash manifest differs from the reviewed layout")
    expected[0x1000000] = ("media.bin", 0x1000000)
    entries = []
    for offset, (filename, maximum) in sorted(expected.items()):
        data = (build / filename).read_bytes()
        if not 0 < len(data) <= maximum:
            parser.error(f"{filename} does not fit its reviewed region")
        if filename in ("storage.bin", "media.bin") and len(data) != maximum:
            parser.error(f"{filename} must cover its complete filesystem partition")
        entries.append({
            "file": Path(filename).name,
            "source": filename,
            "offset": hex(offset),
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "initial_media_only": filename == "media.bin",
        })

    description = json.loads((build / "project_description.json").read_text())
    destination.mkdir(parents=True)
    for entry in entries:
        shutil.copy2(build / entry["source"], destination / entry["file"])
    manifest = {
        "board": "ESP32-S3-Touch-AMOLED-1.75C",
        "version": description["project_version"],
        "status": "compiled candidate; hardware validation pending",
        "flash_bytes": 32 * 1024 * 1024,
        "flash_settings": config["flash_settings"],
        "images": entries,
        "preserved_ranges": [
            {"name": "nvsfactory+nvs", "start": "0x9000", "end_exclusive": "0x10d000"},
            {"name": "phy_init", "start": "0x10f000", "end_exclusive": "0x110000"},
        ],
        "notes": [
            "The layout is incompatible with the original C factory firmware.",
            "Back up the current complete 32 MiB device before any first installation.",
            "Separate addressed files avoid writing padding over NVS; no merged full-flash image is provided.",
            "Preserved raw NVS does not guarantee application-schema or cloud-binding compatibility.",
            "media.bin replaces all media/recordings/history and is omitted from ordinary firmware flashing.",
            "This tool does not access, erase, reset or flash any device.",
        ],
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (destination / "SHA256SUMS").write_text("".join(
        f"{entry['sha256']}  {entry['file']}\n" for entry in entries
    ))
    print(f"Packaged {len(entries)} addressed images into {destination}")


if __name__ == "__main__":
    main()
