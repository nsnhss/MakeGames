# Minimal CMake wrapper for Lua 5.4 source tree.
# Place Lua 5.4.x source files (lua.h, lapi.c, ...) in this directory.

file(GLOB LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lctype.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ldebug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ldo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ldump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lgc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/llex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lobject.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lopcodes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lparser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lstate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lstring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ltable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ltm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lundump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lvm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lzio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lauxlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lbaselib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lcorolib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ldblib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/liolib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lmathlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/loadlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/loslib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lstrlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ltablib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lutf8lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/linit.c
)

list(LENGTH LUA_SOURCES LUA_SOURCE_COUNT)
if(LUA_SOURCE_COUNT EQUAL 0)
    message(FATAL_ERROR "No Lua source files found in ${CMAKE_CURRENT_SOURCE_DIR}. Please place Lua 5.4 source here.")
endif()

add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
