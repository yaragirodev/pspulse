# ---------------------------------------------------------------------------
#  PSPulse Runtime - PSPSDK build
#
#  Usage:
#      make                 # builds EBOOT.PBP into ../bin/PSPRuntime
#      make clean
#      make pspdev-check    # prints the detected PSPSDK location
#
#  Requires PSPSDK (https://github.com/pspdev/pspdev). Set PSPDEV, e.g.
#      export PSPDEV=/usr/local/pspdev
#      export PATH=$PATH:$PSPDEV/bin
# ---------------------------------------------------------------------------

TARGET      = PSPulseRuntime
TITLE       = PSPulse Runtime
PSP_EBOOT_TITLE = $(TITLE)
BUILD_PRX   = 0
PSP_FW_VERSION = 600

OBJS        = Source/RuntimeApp.o Source/SceneRuntime.o Source/RuntimeLua.o \
              Source/generated_assets.o Source/GltfLoaderShim.o ../Shared/Mesh/GltfLoader.o
# GU/GUM are not part of PSPSDK's default LIBS, and the final link runs through
# psp-gcc (not psp-g++), so libstdc++ must be listed explicitly.
LIBS        = -Llib -lpsplua -lpspgu -lpspgum -lpsprtc -lstdc++ -lm
CXXFLAGS    = -O2 -G0 -std=c++17 -DPSP_ENGINE_TINY_JSON -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter -DLUA_32BITS
CFLAGS      = -O2 -G0 -Wall -DLUA_32BITS
INCDIR      = Include Source ../third_party/lua/src/src ../third_party/glm
LIBDIR      = lib
EXTRA_TARGETS = EBOOT.PBP

# Optional XMB artwork (drop files into PSPRuntime/assets/).
ifneq ($(wildcard assets/ICON0.PNG),)
PSP_EBOOT_ICON = assets/ICON0.PNG
endif
ifneq ($(wildcard assets/PIC1.PNG),)
PSP_EBOOT_PIC1 = assets/PIC1.PNG
endif
ifneq ($(wildcard assets/SND0.AT3),)
PSP_EBOOT_SND0 = assets/SND0.AT3
endif

PSPSDK := $(shell psp-config --pspsdk-path 2>/dev/null)

ifeq ($(PSPSDK),)
$(error PSPSDK not found. Install it (https://github.com/pspdev/pspdev) and export \
PSPDEV=/usr/local/pspdev plus PATH=$$PATH:$$PSPDEV/bin, then run `make` again)
endif

include $(PSPSDK)/lib/build.mak

# ---------------------------------------------------------------------------
# Lua 5.4 for the PSP: the runtime links against a static libpsplua.a built
# from the vendored sources. It used to be a leftover build artifact, which
# broke every clean checkout (CI included), so it is generated here.
# ---------------------------------------------------------------------------
LUA_SRC   = ../third_party/lua/src/src
LUA_OBJS  = $(patsubst $(LUA_SRC)/%.c,lib/%.o,$(wildcard $(LUA_SRC)/*.c))
LUA_LIB   = lib/libpsplua.a

$(LUA_LIB): $(LUA_OBJS)
	$(AR) rcs $@ $^

lib/%.o: $(LUA_SRC)/%.c | lib
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@

lib:
	mkdir -p lib

# The PSP ELF must not be linked before the bundled Lua archive exists.
$(TARGET).elf: $(LUA_LIB)

# ---------------------------------------------------------------------------
# The generated asset bundle normally comes from the editor's Build Settings ->
# "Build EBOOT.PBP". For a standalone template build we ship a default one.
# Object compilation is handled by PSPSDK's build.mak pattern rules, which use
# the CFLAGS/CXXFLAGS defined above (INCDIR is folded into them).
# ---------------------------------------------------------------------------
dist: all
	mkdir -p ../bin/PSPRuntime
	cp EBOOT.PBP ../bin/PSPRuntime/EBOOT.PBP
	@echo "EBOOT.PBP -> ../bin/PSPRuntime/EBOOT.PBP"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).prx $(TARGET)_strip.elf EBOOT.PBP PARAM.SFO
	rm -f Source/*.o

.PHONY: all dist clean pspdev-check

pspdev-check:
	@echo "PSPSDK: $(PSPSDK)"
	@psp-gcc --version | head -n 1
