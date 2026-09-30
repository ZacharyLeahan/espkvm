#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the Xbox profile with optional, ignored .env Wi-Fi defaults."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
KEYS = {"WIFI_SSID": "CONFIG_KVM_WIFI_DEFAULT_SSID",
        "WIFI_PASSWORD": "CONFIG_KVM_WIFI_DEFAULT_PASSWORD"}


def parse_env(text):
    values = dict.fromkeys(KEYS, "")
    seen = set()
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key, value = key.strip(), value.strip()
        if not separator or key not in KEYS or key in seen:
            raise ValueError(f"Invalid or duplicate key on .env line {number}")
        seen.add(key)
        if value.startswith('"'):
            try:
                value = json.loads(value)
            except json.JSONDecodeError:
                raise ValueError(f"Invalid quoted value on .env line {number}") from None
        elif value.startswith("'"):
            if len(value) < 2 or not value.endswith("'"):
                raise ValueError(f"Invalid quoted value on .env line {number}")
            value = value[1:-1]
        if not isinstance(value, str) or any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError(f"Control characters are not allowed on .env line {number}")
        values[key] = value
    ssid, password = values["WIFI_SSID"], values["WIFI_PASSWORD"]
    if len(ssid.encode()) > 32:
        raise ValueError("Wi-Fi SSID must be at most 32 bytes")
    if password and not 8 <= len(password.encode()) <= 63:
        raise ValueError("Wi-Fi password must be empty or 8-63 bytes")
    if password and not ssid:
        raise ValueError("A Wi-Fi password requires an SSID")
    return values


def merge_settings(text, values):
    # Replace only our two defaults. Existing device NVS is intentionally untouched.
    keys = "|".join(KEYS.values())
    kept = [line for line in text.splitlines()
            if not re.match(rf"^(?:{keys})=|^# (?:{keys}) is not set$", line)]
    return "\n".join(kept) + "\n" + "".join(
        f"{KEYS[key]}={json.dumps(value, ensure_ascii=False)}\n"
        for key, value in values.items())


def private_write(path, text):
    if path.is_symlink():
        raise ValueError("Refusing to write through a configuration symlink")
    with path.open("w", encoding="utf-8") as output:
        os.chmod(path, 0o600)
        output.write(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    if not shutil.which("idf.py"):
        raise ValueError("ESP-IDF is not active; run '. tools/env.sh' first")
    dotenv = ROOT / ".env"
    values = parse_env(dotenv.read_text(encoding="utf-8") if dotenv.exists() else "")
    os.umask(0o077)
    build = ROOT / "build.xbox-local"
    if build.is_symlink():
        raise ValueError("Refusing a symlinked local build directory")
    build.mkdir(mode=0o700, exist_ok=True)
    os.chmod(build, 0o700)
    generated = build / "wifi.private.defaults"
    private_write(generated, merge_settings("# Generated locally; never publish.\n", values))
    config = build / "sdkconfig"
    if config.exists():
        private_write(config, merge_settings(config.read_text(encoding="utf-8"), values))
    defaults = [ROOT / "sdkconfig.defaults", ROOT / "boards/funcev_p4.defaults",
                ROOT / "boards/funcev_xbox.defaults", generated]
    print("Building in ignored build.xbox-local; credentials are not printed.", flush=True)
    print("Do not publish firmware or build files containing your Wi-Fi credentials.", flush=True)
    command = ["idf.py", "-B", str(build), "-D", f"SDKCONFIG={config}",
               "-D", "SDKCONFIG_DEFAULTS=" + ";".join(map(str, defaults)),
               "reconfigure" if args.configure_only else "build"]
    return subprocess.call(command, cwd=ROOT)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        print(f"Build setup failed: {error}", file=sys.stderr)
        sys.exit(1)
