cmake_minimum_required(VERSION 3.15)
project(MakeGames VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 外部 SDK 路径
# ---------------------------------------------------------------------------
# 优先使用环境变量 VULKAN_SDK（标准安装会自动设置），否则回退到默认路径。
# 也可用 -DVULKAN_SDK=... 显式覆盖。
if(DEFINED ENV{VULKAN_SDK})
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK_ENV)
    set(VULKAN_SDK "${VULKAN_SDK_ENV}" CACHE PATH "Vulkan SDK 安装路径")
else()
    set(VULKAN_SDK "C:/VulkanSDK/1.4.341.1" CACHE PATH "Vulkan SDK 安装路径")
endif()
if(NOT EXISTS "${VULKAN_SDK}/Include/vulkan/vulkan.h")
    message(FATAL_ERROR "Vulkan SDK 未找到：${VULKAN_SDK}\n请安装 Vulkan SDK 或用 -DVULKAN_SDK=... 指定路径。")
endif()
set(SDL2_PATH "${CMAKE_SOURCE_DIR}/SDL2-2.32.10/x86_64-w64-mingw32" CACHE PATH "SDL2 MinGW 路径")

add_compile_definitions(SDL_MAIN_HANDLED)

# 确保 UTF-8 字符串字面量正确编译（MinGW 默认使用系统本地编码）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ---------------------------------------------------------------------------
# 第三方库：FreeType
# ---------------------------------------------------------------------------
add_subdirectory(third_party/freetype)

# ---------------------------------------------------------------------------
# 第三方库：Lua
# 优先使用本地 third_party/lua；缺失时回退到 FetchContent.
# ---------------------------------------------------------------------------
set(LUA_TARGET "")

set(LOCAL_LUA_DIR ${CMAKE_SOURCE_DIR}/third_party/lua)

if(EXISTS ${LOCAL_LUA_DIR}/lua.h)
    add_subdirectory(${LOCAL_LUA_DIR} ${CMAKE_BINARY_DIR}/third_party/lua)
    set(LUA_TARGET lua_static)
    message(STATUS "Using local Lua from third_party/")
else()
    include(FetchContent)
    FetchContent_Declare(lua GIT_REPOSITORY https://github.com/lua/lua.git GIT_TAG v5.4.6)
    FetchContent_MakeAvailable(lua)
    foreach(tgt lua_static lua::lua liblua lua)
        if(TARGET ${tgt})
            set(LUA_TARGET ${tgt})
            message(STATUS "Found Lua target: ${LUA_TARGET}")
            break()
        endif()
    endforeach()
    if(NOT LUA_TARGET)
        message(FATAL_ERROR "Lua target not found.")
    endif()
endif()

# ---------------------------------------------------------------------------
# 全局共享：ImGui 源文件
# ---------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# ---------------------------------------------------------------------------
# 全局共享：头文件 / 链接目录 / 库
# ---------------------------------------------------------------------------
set(MG_INCLUDE_DIRS
    ${SDL2_PATH}/include/SDL2
    ${VULKAN_SDK}/Include
    ${IMGUI_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/freetype/include
    ${CMAKE_SOURCE_DIR}/third_party/lua
    CACHE INTERNAL "引擎全局包含目录"
)

set(MG_LINK_DIRS
    ${SDL2_PATH}/lib
    ${VULKAN_SDK}/Lib
    CACHE INTERNAL "引擎全局链接目录"
)

set(MG_LINK_LIBS
    SDL2
    vulkan-1
    imm32
    freetype
    CACHE INTERNAL "引擎全局链接库"
)

# ---------------------------------------------------------------------------
# 子模块（分级构建）
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(launcher)
add_subdirectory(editor)

# 单元测试（构建 engine_tests 目标后用 ctest 运行）
enable_testing()
add_subdirectory(tests)


