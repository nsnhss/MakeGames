# ---------------------------------------------------------------------------
# Editor 可执行文件
# ---------------------------------------------------------------------------
# 显式列出源文件（不用 GLOB，新增文件需手动加入并重新配置）
set(EDITOR_PANEL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/editor/panels/console_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/details_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/event_editor_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/item_database_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/outliner_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/script_editor_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/animation_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/tile_palette_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/tile_autotile.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/viewport_panel.cpp
)

set(EDITOR_SOURCES
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/command_history.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/viewport_state.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/tileset_editor.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/scene_session.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/prefab_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/spritesheet_import.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_project_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_app.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_application.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/viewport_render_target.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_context.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_layer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_menu_bar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/editor_tool_bar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/export_game.cpp
    ${EDITOR_PANEL_SOURCES}
)

add_executable(editor ${EDITOR_SOURCES})

target_include_directories(editor PRIVATE
    ${MG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

target_link_directories(editor PRIVATE
    ${MG_LINK_DIRS}
)

target_link_libraries(editor PRIVATE
    engine_runtime
    engine_editor
    engine_gameui
    ${LUA_TARGET}
)

target_link_options(editor PRIVATE -mwindows -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)

# 构建后复制 SDL2.dll
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SDL2_PATH}/bin/SDL2.dll
    $<TARGET_FILE_DIR:editor>
)

# 构建后复制字体资源
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/fonts
    $<TARGET_FILE_DIR:editor>/assets/fonts
)

# 构建后复制着色器资源
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/shaders
    $<TARGET_FILE_DIR:editor>/assets/shaders
)
