#!/usr/bin/env python3
"""Build and run service fault-injection tests without ESP-IDF or device access."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="storage-host-tests-") as directory:
    executable = str(Path(directory) / "test_storage_service")
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-D_POSIX_C_SOURCE=200809L",
        "-Wall", "-Wextra", "-Werror", "-pthread", "-g",
        "-fsanitize=address,undefined", "-I" + str(here / "stubs"),
        "-I" + str(here.parent / "include"), str(here / "test_storage_service.c"),
        "-o", executable,
    ], check=True)
    subprocess.run([executable], check=True)
