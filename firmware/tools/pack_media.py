#!/usr/bin/env python3
"""Pack a directory into a 16 MiB 1.75C media image, without device access."""

import argparse
import os
from pathlib import Path
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.directory.resolve(strict=True)
    output = args.output.resolve()
    if not source.is_dir():
        parser.error("directory must be a folder")
    if output == source or source in output.parents:
        parser.error("output must be outside the input folder")
    if output.exists():
        parser.error("output already exists; choose a new output file")
    for path in source.rglob("*"):
        if path.is_symlink():
            parser.error("symbolic links are not accepted in media folders")
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        parser.error("activate ESP-IDF 5.5 before running this tool")
    sys.path.insert(0, str(Path(idf_path) / "components" / "fatfs"))
    from wl_fatfsgen import WLFATFS

    # Match sdkconfig: FAT and wear levelling both use 4096-byte sectors.
    # The IDF generator assigns a fresh FAT volume serial on each build.
    image = WLFATFS(size=16 * 1024 * 1024, sector_size=4096,
                   fat_tables_cnt=2, long_names_enabled=True,
                   use_default_datetime=True, device_id=0x175C2026)
    image.plain_fatfs.generate(str(source))
    image.init_wl()
    output.parent.mkdir(parents=True, exist_ok=True)
    image.wl_write_filesystem(str(output))
    print(f"Created {output} ({output.stat().st_size} bytes). No device was accessed.")


if __name__ == "__main__":
    main()
