Import("env")

import os
import re

from platformio.project.exception import InvalidProjectConfError


try:
    pair_key = env.GetProjectOption("custom_panel_pair_key")
except InvalidProjectConfError:
    pair_key = env.GetProjectOption("panel_pair_key")
if not re.fullmatch(r"[0-9A-Fa-f]{32}", pair_key or ""):
    raise ValueError("panel_pair_key must be exactly 16 bytes of hexadecimal")
if int(pair_key, 16) == 0:
    raise ValueError("panel_pair_key must not be all zero")

build_dir = env.subst("$BUILD_DIR")
header_path = os.path.join(build_dir, "panel_credentials.h")
env.Append(CPPPATH=[build_dir])
key_bytes = ", ".join("0x{}".format(pair_key[index:index + 2]) for index in range(0, 32, 2))
header = """#pragma once

#include <stdint.h>

namespace PanelCredentials {
constexpr uint8_t K_PAIR[16] = {""" + key_bytes + """};
} // namespace PanelCredentials
"""

os.makedirs(build_dir, exist_ok=True)
temporary_path = header_path + ".tmp"
with open(temporary_path, "w") as output:
    output.write(header)
os.chmod(temporary_path, 0o600)
os.replace(temporary_path, header_path)
