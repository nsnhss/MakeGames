# ---------------------------------------------------------------------------
# 引擎单元测试（自包含最小框架，零第三方依赖）
# 只测纯逻辑：ECS / 序列化 / 资源数据库，不碰 Vulkan/SDL 窗口。
# ---------------------------------------------------------------------------
add_executable(engine_tests
    test_sparse_set.cpp
    test_world.cpp
    test_serializer.cpp
    test_databases.cpp
    test_sprite_batch.cpp
    test_atlas_packer.cpp
    test_audio_mixer.cpp
    test_collision_merge.cpp
    test_lua_bindings.cpp
    test_tween_animation.cpp
    test_map_settings.cpp
    test_actor_curves.cpp
    test_tile_animation.cpp
    test_spritesheet_grid.cpp
    test_event_triggers.cpp
)

target_include_directories(engine_tests PRIVATE
    ${MG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(engine_tests PRIVATE
    ${MG_LINK_DIRS}
)

target_link_libraries(engine_tests PRIVATE
    engine_runtime
    ${LUA_TARGET}
)

add_test(NAME engine_tests COMMAND engine_tests)
