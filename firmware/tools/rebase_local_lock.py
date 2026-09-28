#!/usr/bin/env python3
"""Rebase only bundled local dependency paths after moving this checkout.

ESP-IDF stores absolute paths in dependencies.lock and validates them before
resolving manifests. Keep all registry pins and hashes, and replace only the
five known local path scalars; do not regenerate or reformat the lock file.
"""

import argparse
import json
import os
from pathlib import Path
import stat
import tempfile

import yaml


LOCAL_COMPONENTS = {
    "brookesia_core": "components/brookesia_core",
    "chmorgan/esp-audio-player": "firmware/components/esp_audio_player_safe/esp-audio-player",
    "espressif/avi_player": "firmware/components/avi_player_safe/avi_player",
    "espressif/esp_xiaozhi": "firmware/components/esp_xiaozhi_safe/esp_xiaozhi",
    "waveshare/esp32_s3_touch_amoled_1_75": "firmware/components/waveshare__esp32_s3_touch_amoled_1_75",
}


def mapping(node, context):
    if not isinstance(node, yaml.MappingNode):
        raise ValueError(f"Expected YAML mapping for {context}")
    result = {}
    for key, value in node.value:
        if not isinstance(key, yaml.ScalarNode):
            raise ValueError(f"Expected scalar YAML key in {context}")
        if key.value in result:
            raise ValueError(f"Duplicate YAML key {key.value!r} in {context}")
        result[key.value] = value
    return result


def rebase_lock(firmware_dir: Path) -> bool:
    firmware_dir = firmware_dir.resolve()
    lock = firmware_dir / "dependencies.lock"
    try:
        original = lock.read_bytes().decode("utf-8")
    except FileNotFoundError:
        return False  # Allow the component manager to create a first lock file.
    document = mapping(yaml.compose(original, Loader=yaml.SafeLoader), "lock file")
    dependencies = mapping(document.get("dependencies"), "dependencies")
    replacements = []
    found = set()
    for name, node in dependencies.items():
        component = mapping(node, name)
        source = mapping(component.get("source"), f"{name} source")
        source_type = source.get("type")
        if not isinstance(source_type, yaml.ScalarNode) or source_type.value != "local":
            continue
        if name not in LOCAL_COMPONENTS:
            raise ValueError(f"Unknown local component {name!r}; update the explicit path mapping")
        found.add(name)
        path = source.get("path")
        if not isinstance(path, yaml.ScalarNode) or path.tag != "tag:yaml.org,2002:str":
            raise ValueError(f"Missing or invalid local path for {name}")
        directory = (firmware_dir.parent / LOCAL_COMPONENTS[name]).resolve()
        if not all((directory / filename).is_file() for filename in ("CMakeLists.txt", "idf_component.yml")):
            raise ValueError(f"Missing bundled component {name}: {directory}")
        if path.value != str(directory):
            replacements.append((path.start_mark.index, path.end_mark.index,
                                 json.dumps(str(directory), ensure_ascii=False)))
    missing = LOCAL_COMPONENTS.keys() - found
    if missing:
        raise ValueError(f"Missing local component entries: {', '.join(sorted(missing))}")
    if not replacements:
        return False

    updated = original
    for start, end, value in sorted(replacements, reverse=True):
        updated = updated[:start] + value + updated[end:]
    # Verify the edited YAML has exactly the intended semantic changes too.
    expected = yaml.safe_load(original)
    for name, relative in LOCAL_COMPONENTS.items():
        expected["dependencies"][name]["source"]["path"] = str((firmware_dir.parent / relative).resolve())
    if yaml.safe_load(updated) != expected:
        raise ValueError("Local path replacement would alter unrelated lock data")

    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=lock.parent, prefix=".dependencies.lock-", delete=False) as output:
            temporary = Path(output.name)
            output.write(updated.encode("utf-8"))
            output.flush()
            os.fsync(output.fileno())
        temporary.chmod(stat.S_IMODE(lock.stat().st_mode))
        os.replace(temporary, lock)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware_dir", type=Path, nargs="?", default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        if rebase_lock(args.firmware_dir):
            print("Updated bundled local dependency paths; registry versions and hashes preserved.")
    except (OSError, ValueError, yaml.YAMLError) as error:
        parser.exit(1, f"Cannot rebase local dependency lock: {error}\n")


if __name__ == "__main__":
    main()
