# ---------------------------------------------------------------------------
# engine_runtime：纯运行时库（零 ImGui 依赖）
# ---------------------------------------------------------------------------
add_library(engine_runtime STATIC
    platform/app_bootstrap.cpp
    platform/input.cpp
    resource/resource_manager.cpp
    resource/atlas_packer.cpp
    resource/data_manager.cpp
    resource/item_database.cpp
    resource/actor_database.cpp
    resource/skill_database.cpp
    resource/enemy_database.cpp
    resource/shop_database.cpp
    resource/spritesheet_grid.cpp
    core/log.cpp
    core/timer.cpp
    renderer/renderer_context.cpp
    renderer/swapchain.cpp
    renderer/shader.cpp
    renderer/buffer.cpp
    renderer/pipeline.cpp
    renderer/descriptor.cpp
    renderer/texture.cpp
    app/layer_stack.cpp
    app/application.cpp
    scene/world.cpp
    scene/component.cpp
    scene/event_system.cpp
    scene/event_system_battle.cpp
    scene/event_system_shop.cpp
    scene/scene_serializer.cpp
    scene/systems/camera_system.cpp
    scene/systems/collision_merge.cpp
    scene/map_settings.cpp
    scene/systems/sprite_render_system.cpp
    scene/systems/sprite_batch_builder.cpp
    scene/systems/tilemap_render_system.cpp
    scene/systems/animator_system.cpp
    scene/systems/tween_animation_system.cpp
    scene/systems/tile_animation.cpp
    audio/audio_manager.cpp
    audio/audio_decoder.cpp
    audio/audio_codec_impl.c
    audio/audio_mixer.cpp
    scene/scene.cpp
    script/script_engine.cpp
)

target_include_directories(engine_runtime PUBLIC
    ${MG_INCLUDE_DIRS}
)

target_link_directories(engine_runtime PUBLIC
    ${MG_LINK_DIRS}
)

target_link_libraries(engine_runtime PUBLIC
    ${MG_LINK_LIBS}
    ${LUA_TARGET}
)

# ---------------------------------------------------------------------------
# engine_editor：编辑器 / Launcher 共用的 ImGui 封装
# mg_imgui_backend.cpp 依赖 Vulkan + SDL2，需要链接 ${MG_LINK_LIBS}
# ---------------------------------------------------------------------------
add_library(engine_editor STATIC
    editor/imgui/mg_imgui_theme.cpp
    editor/imgui/mg_imgui_extensions.cpp
    editor/imgui/mg_imgui_panel.cpp
    editor/imgui/mg_imgui_backend.cpp
    editor/file_dialog.cpp
)

target_include_directories(engine_editor PUBLIC
    ${MG_INCLUDE_DIRS}
)

target_link_libraries(engine_editor PUBLIC
    engine_runtime
    freetype
    ${MG_LINK_LIBS}
    ${LUA_TARGET}
)

# ---------------------------------------------------------------------------
# game.exe：独立游戏运行时入口
# ---------------------------------------------------------------------------
add_executable(game game_main.cpp)

target_include_directories(game PRIVATE
    ${MG_INCLUDE_DIRS}
)

target_link_libraries(game PRIVATE
    engine_runtime
    ${LUA_TARGET}
)

# 静态链接 MinGW runtime，避免发布时额外携带 libgcc/libstdc++/winpthread DLL
target_link_options(game PRIVATE
    -mwindows
    -static-libgcc
    -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
)

# 构建后复制 SDL2.dll 到 game.exe 目录（发布导出时会再次复制）
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SDL2_PATH}/bin/SDL2.dll
    $<TARGET_FILE_DIR:game>
)

# 构建后复制着色器到 game.exe 目录
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/shaders
    $<TARGET_FILE_DIR:game>/assets/shaders
)
