# ---------------------------------------------------------------------------
# Launcher 可执行文件
# ---------------------------------------------------------------------------
set(LAUNCHER_SOURCES
    main.cpp
    launcher_application.cpp
    launcher_layer.cpp
    pending_editor.cpp
    core/project_manager.cpp
    ui/launcher_window.cpp
    ui/launcher_ui_kit.cpp
    ui/launcher_path_utils.cpp
    ui/launcher_home_page.cpp
    ui/launcher_projects_page.cpp
    ui/launcher_misc_pages.cpp
    ui/launcher_dialogs.cpp
    ui/log_panel.cpp
)

add_executable(launcher ${LAUNCHER_SOURCES} ${IMGUI_SOURCES})

target_include_directories(launcher PRIVATE
    ${MG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/launcher
)

target_link_directories(launcher PRIVATE
    ${MG_LINK_DIRS}
)

target_link_libraries(launcher PRIVATE
    engine_runtime
    engine_editor
    engine_gameui
    ${LUA_TARGET}
)

target_link_options(launcher PRIVATE -mwindows -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)

# 构建后复制 SDL2.dll
add_custom_command(TARGET launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SDL2_PATH}/bin/SDL2.dll
    $<TARGET_FILE_DIR:launcher>
)

# 构建后复制字体资源
add_custom_command(TARGET launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/fonts
    $<TARGET_FILE_DIR:launcher>/assets/fonts
)
