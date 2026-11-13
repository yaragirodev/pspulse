# Lua 5.4.6 for PlayStation Portable

Vendored copy of [Lua 5.4.6](https://www.lua.org/ftp/lua-5.4.6.tar.gz) with a
single portability patch:

* `src/src/luaconf.h`: `LUA_32BITS` is wrapped into `#ifndef` so it can be
  enabled from the command line. The Allegrex (`psp-gcc`) toolchain is built
  without 64-bit `long long` support and the VFPU is single precision, so the
  PSP runtime always compiles Lua with `-DLUA_32BITS` (32-bit integers, float
  numbers). This is also the fastest configuration on real hardware.

Build the static library with:

```bash
export PSPDEV=/usr/local/pspdev
export PATH=$PATH:$PSPDEV/bin
./BuildPspLua.sh          # -> PSPRuntime/lib/libpsplua.a
```
