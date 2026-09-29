#!/usr/bin/env python3
"""Execute real Brookesia lifecycle method bodies against failure-injected APIs.

The production sources have heavy LVGL/ESP-IDF includes. Extract their complete
method definitions without altering them, and substitute only platform/UI
dependencies with the small deterministic harness. This checks actual lifecycle
control flow, rather than duplicating the implementation in a model or asserting
source strings. SDK compilation separately validates the complete source units.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "components/brookesia_core/systems/base"


def definition(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main() -> None:
    app = (BASE / "esp_brookesia_base_app.cpp").read_text()
    manager = (BASE / "esp_brookesia_base_manager.cpp").read_text()
    app_header = (BASE / "esp_brookesia_base_app.hpp").read_text()
    methods = [definition(app, name) for name in (
        "bool App::processRun()", "bool App::processClose(bool is_app_active)")]
    methods += [definition(manager, name) for name in (
        "bool Manager::startApp(int id)", "bool Manager::processAppRun(App *app)",
        "bool Manager::processAppClose(App *app)")]
    app_declarations = definition(app_header, "enum class Status") + ";\n"
    app_declarations += "Status _status = Status::CLOSED;\n"
    # Use the flags and stage enum from the real headers so adding a lifecycle
    # flag cannot silently leave the fixture testing an obsolete declaration.
    flags = re.search(r"struct \{\s*uint8_t is_closing: 1;.*?\} _flags = \{\};", app_header, re.S)
    if flags is None:
        raise RuntimeError("App lifecycle flags were not found")
    app_declarations += flags.group(0)
    app_declarations += definition(app_header, "enum class CloseStage") + ";\n"
    app_declarations += "CloseStage _close_stage = CloseStage::APP;\n"
    app_declarations += "bool _manager_close_in_progress = false;\n"
    harness = (ROOT / "tests/support/base_lifecycle_harness.cpp").read_text()
    harness = harness.replace("APP_LIFECYCLE_DECLARATIONS", app_declarations)
    harness = harness.replace("PRODUCTION_LIFECYCLE_METHODS", "\n\n".join(methods))
    with tempfile.TemporaryDirectory(prefix="brookesia-lifecycle-") as output:
        source = Path(output) / "lifecycle.cpp"
        binary = Path(output) / "lifecycle"
        source.write_text(harness)
        command = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run(command + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                                  str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
