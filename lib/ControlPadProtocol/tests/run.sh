#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${CXX:-c++}
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/control-pad-protocol.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

common_flags="-std=c++11 -Wall -Wextra -Werror"
sources="$root/src/ControlPadProtocol.cpp $root/tests/test_ControlPadProtocol.cpp"
includes="-I$root/include"

if "$compiler" $common_flags -fsanitize=address,undefined -fno-omit-frame-pointer \
    $includes $sources -o "$build_dir/ControlPadProtocol" >/dev/null 2>&1; then
  printf '%s\n' 'Sanitizers: address,undefined'
else
  "$compiler" $common_flags $includes $sources -o "$build_dir/ControlPadProtocol"
  printf '%s\n' 'Sanitizers: unavailable; ran without them'
fi

node "$root/tests/verify_command_kat.js"
"$build_dir/ControlPadProtocol"
