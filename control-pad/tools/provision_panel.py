#!/usr/bin/env python3
"""Create one ignored, local control-pad pairing profile without exposing its key."""

import binascii
import os
import re
import secrets
import sys
from pathlib import Path


PROFILE_NAME = re.compile(r"[a-z][a-z0-9_-]{0,31}\Z")
ENV_SECTION = re.compile(r"^\s*\[env:([^\]]+)\]\s*$", re.MULTILINE)
ROOT = Path(__file__).resolve().parent.parent.parent
CONTROL_PAD = ROOT / "control-pad"
PLATFORMIO_CONFIG = CONTROL_PAD / "platformio.ini"
LOCAL_PROFILE = CONTROL_PAD / "platformio.local.ini"
ARTIFACTS = ROOT / ".artifacts" / "panels"


def atomic_create(path: Path, content: str, mode: int) -> None:
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    try:
        with os.fdopen(descriptor, "w") as output:
            output.write(content)
            output.flush()
            os.fsync(output.fileno())
    except BaseException:
        try:
            path.unlink()
        except FileNotFoundError:
            pass
        raise


def atomic_replace(path: Path, content: str, mode: int) -> None:
    temporary = path.with_name(".{}.tmp-{}".format(path.name, os.getpid()))
    try:
        atomic_create(temporary, content, mode)
        os.replace(temporary, path)
        os.chmod(path, mode)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def committed_environment_names() -> frozenset[str]:
    return frozenset(ENV_SECTION.findall(PLATFORMIO_CONFIG.read_text()))


def main() -> int:
    if len(sys.argv) != 2 or not PROFILE_NAME.fullmatch(sys.argv[1]):
        print("Profile name must match [a-z][a-z0-9_-]{0,31}", file=sys.stderr)
        return 2

    name = sys.argv[1]
    if name in committed_environment_names():
        print("Profile name conflicts with a committed environment", file=sys.stderr)
        return 2

    environment = name
    artifact = ARTIFACTS / (name + ".pairing.txt")
    local_content = LOCAL_PROFILE.read_text() if LOCAL_PROFILE.exists() else ""

    if "[env:{}]".format(environment) in local_content or artifact.exists():
        print("Profile already exists; refusing to overwrite", file=sys.stderr)
        return 1

    # Pairing identity requires only K_pair. secrets.token_bytes uses the OS CSPRNG.
    key = secrets.token_bytes(16)
    key_hex = key.hex().upper()
    crc = binascii.crc32(b"\x01" + key) & 0xFFFFFFFF
    # env:profile is the production ESP-NOW source; diag/input stay radio-free.
    profile_entry = "\n[env:{}]\nextends = env:profile\ncustom_panel_pair_key = {}\n".format(environment, key_hex)
    artifact_content = "GLCP1-{}-{:08X}\n".format(key_hex, crc)

    ARTIFACTS.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(ARTIFACTS, 0o700)
    atomic_create(artifact, artifact_content, 0o600)
    try:
        atomic_replace(LOCAL_PROFILE, local_content + profile_entry, 0o600)
    except BaseException:
        artifact.unlink()
        raise

    print("Provisioned profile '{}' and pairing artifact.".format(name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
