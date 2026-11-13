#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  Builds Lua 5.4 for the PSP (Allegrex) and produces a static library that is
#  linked into the PSP runtime / EBOOT.PBP.
#
#  Output: PSPRuntime/lib/libpsplua.a
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
LUA_SRC="$SCRIPT_DIR/src/src"
OUT_DIR="$ROOT_DIR/PSPRuntime/lib"

if ! command -v psp-gcc >/dev/null 2>&1; then
    echo "error: psp-gcc not found. Install PSPSDK and export PSPDEV/PATH first." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
OBJECTS=()
for src in "$LUA_SRC"/*.c; do
    name="$(basename "$src" .c)"
    # The standalone interpreter / compiler binaries are not needed.
    case "$name" in
        lua|luanone|luac) continue ;;
    esac
    obj="$OUT_DIR/$name.o"
    echo "  CC  $name.c"
    # LUA_32BITS selects `int`/`float` numbers: the Allegrex compiler is built
    # without 64-bit `long long` support and the VFPU is single precision, so
    # this is also the fastest configuration on real hardware.
    psp-gcc -O2 -G0 -DLUA_32BITS -DLUA_USE_LONGJMP -std=gnu99 -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

psp-ar rcs "$OUT_DIR/libpsplua.a" "${OBJECTS[@]}"
echo "==> $OUT_DIR/libpsplua.a"
