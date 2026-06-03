#!/bin/bash
# Host test wrapper: include butano's headers.
# Usage: ./tests/build_host.sh <source.cpp> [extra sources...] -o <out>
#
# Example:
#   ./tests/build_host.sh src/vocab.cpp tests/test_vocab.cpp -o /tmp/test_vocab
#
# Adds butano's include paths to g++ so test files can #include
# butano headers (which is fine — they're just C++ headers, no
# GBA-only code). Tests use the __GBA__ guard to avoid linking
# GBA-specific code on host.

set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"

g++ -std=c++17 -Wall -Wextra \
    -I"$HERE/include" \
    -I/home/hlm/butano/butano/include \
    -I/home/hlm/butano/butano/hw/include \
    -I/home/hlm/butano/butano/hw/3rd_party/libtonc/include \
    -I/home/hlm/butano/butano/hw/3rd_party/libugba/include \
    "$@"
