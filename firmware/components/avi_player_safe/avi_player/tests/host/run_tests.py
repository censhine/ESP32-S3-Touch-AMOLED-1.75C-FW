#!/usr/bin/env python3
"""Run AVI host tests with a C11/pthreads compiler (CC, or cc by default)."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", nargs="?", type=Path, help="optional SD AVI fixture")
    args = parser.parse_args()
    fixture = args.fixture.resolve(strict=True) if args.fixture else None
    host_root = Path(__file__).resolve().parent
    component_root = host_root.parent.parent
    compiler = shlex.split(os.environ.get("CC", "cc"))
    common = [
        "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-pthread",
        "-DAVI_PLAYER_VER_MAJOR=2", "-DAVI_PLAYER_VER_MINOR=0", "-DAVI_PLAYER_VER_PATCH=0",
        "-I", str(host_root / "stubs"), "-I", str(component_root / "include"),
        "-I", str(host_root),
    ]
    compiler_version = subprocess.run(compiler + ["--version"], check=True,
                                      capture_output=True, text=True).stdout
    if "clang" in compiler_version.lower():
        # ESP32's 32-bit LONG_MAX guard is redundant on 64-bit macOS hosts.
        common.append("-Wno-error=tautological-constant-out-of-range-compare")
    with tempfile.TemporaryDirectory(prefix="avi-player-safe-") as build_dir:
        build_root = Path(build_dir)
        objects = []
        for source in [component_root / "avi_player.c", component_root / "avifile.c",
                       host_root / "host_runtime.c", host_root / "avi_player_host_test.c"]:
            output = build_root / (source.stem + ".o")
            wrappers = ["-Dfopen=host_tracked_fopen", "-Dfclose=host_tracked_fclose",
                        "-Dfread=host_tracked_fread"] if source.name == "avi_player.c" else []
            subprocess.run(compiler + common + wrappers + ["-c", str(source), "-o", str(output)],
                           check=True, cwd=build_root)
            objects.append(str(output))
        executable = build_root / "avi_player_host_test"
        subprocess.run(compiler + ["-pthread"] + objects + ["-o", str(executable)], check=True)
        subprocess.run([str(executable)] + ([str(fixture)] if fixture else []),
                       check=True, cwd=build_root)


if __name__ == "__main__":
    main()
