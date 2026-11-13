# Copies PSPRuntime/lib (libpsplua.a + Lua objects) next to the editor so that
# Build Settings can link Lua into an EBOOT without rebuilding it.
#
# The directory is a *build output* of PSPRuntime/Makefile, so a clean checkout
# (CI, fresh clone) does not have it. Missing it is not an error: the build
# pipeline compiles Lua from third_party/lua on demand.

if(EXISTS "${PSPRUNTIME_SRC}")
    file(GLOB _lua_files "${PSPRUNTIME_SRC}/*")
    if(_lua_files)
        file(MAKE_DIRECTORY "${DEST}")
        file(COPY ${_lua_files} DESTINATION "${DEST}")
        list(LENGTH _lua_files _count)
        message(STATUS "Staged ${_count} Lua build artifacts into ${DEST}")
    endif()
else()
    message(STATUS "PSPRuntime/lib not built yet - the editor will compile Lua on demand")
endif()
