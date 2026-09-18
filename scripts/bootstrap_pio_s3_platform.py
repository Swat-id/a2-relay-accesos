#!/usr/bin/env python3
"""
Create .pio-kincony-platform/ from the installed espressif32@6.8.1 platform and
add the framework-arduinoespressif32-libs entry required by Arduino-ESP32 3.x
(ETH W5500). Stock PlatformIO 6.8 does not list this package in platform.json.
Run once per machine after installing platform espressif32@6.8.1:
  python3 scripts/bootstrap_pio_s3_platform.py
"""
from __future__ import annotations

import json
import os
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DST = REPO / ".pio-kincony-platform"
SRC = Path(os.path.expanduser("~/.platformio/platforms/espressif32@6.8.1"))

# Pin from package_esp32_index.json (Arduino 3.0.7) — hosted on esp32-arduino-lib-builder
LIBS_SPEC = {
    "type": "framework",
    "optional": True,
    "owner": "espressif",
    "version": "https://github.com/espressif/esp32-arduino-lib-builder/releases/download/idf-release_v5.1/esp32-arduino-libs-idf-release_v5.1-632e0c2a.zip",
}


def main() -> int:
    if not SRC.is_dir():
        print(f"Not found: {SRC}\nInstall first:  pio platform install espressif32@6.8.1", file=sys.stderr)
        return 1
    if DST.exists():
        shutil.rmtree(DST)
    shutil.copytree(SRC, DST)
    p = DST / "platform.json"
    data = json.loads(p.read_text(encoding="utf-8"))
    pkgs = data.setdefault("packages", {})
    if "framework-arduinoespressif32-libs" in pkgs:
        print("framework-arduinoespressif32-libs already in platform.json; refreshed copy.")
    pkgs["framework-arduinoespressif32-libs"] = LIBS_SPEC
    p.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    print(f"Wrote {DST} (patched for Arduino 3.0.7 + libs).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
