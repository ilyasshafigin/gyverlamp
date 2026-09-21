#!/bin/sh
set -eu

temporary=${TMPDIR:-/tmp}/control-pad-radio-$$
trap 'rm -f "$temporary" "$temporary.sanitize"' EXIT
flags='-std=c++11 -Wall -Wextra -Werror -Icontrol-pad/include'
if c++ $flags -fsanitize=address,undefined -c control-pad/tests/test_panel_radio_logic.cpp -o "$temporary.sanitize" >/dev/null 2>&1; then
  flags="$flags -fsanitize=address,undefined"
  rm -f "$temporary.sanitize"
fi
c++ $flags control-pad/tests/test_panel_radio_logic.cpp control-pad/src/radio/panel_radio_runtime.cpp control-pad/src/radio/panel_radio_binding.cpp -o "$temporary"
"$temporary"
