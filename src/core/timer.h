#pragma once
#include <cstdint>

namespace mg {

// 计时器：提供 DeltaTime、FPS、总运行时间
class Timer {
public:
    Timer();

    // 每帧调用一次，更新内部计时状态
    void Tick();

    // 距离上一帧的秒数（已限制最大值，防止断点/拖拽时 dt 爆炸）
    float GetDeltaTime() const { return delta_time_; }

    // 总运行时间（秒）
    float GetTotalTime() const { return total_time_; }

    // 当前 FPS（基于最近 0.5 秒的平均值）
    float GetFPS() const { return fps_; }

    // 自上一帧以来的帧数（固定为 1，除非 Tick 被多次调用）
    uint32_t GetFrameCount() const { return frame_count_; }

    // 限制最大 delta time，防止调试断点导致 dt 过大
    void SetMaxDeltaTime(float max_dt) { max_delta_time_ = max_dt; }

    // 设置零 delta 时的默认帧间隔（默认 1/60）
    void SetDefaultDeltaTime(float dt) { default_delta_time_ = dt; }

private:
    uint64_t perf_freq_ = 0;
    uint64_t last_time_ = 0;
    uint64_t frame_count_ = 0;

    float delta_time_ = 0.0f;
    float total_time_ = 0.0f;
    float max_delta_time_ = 0.5f;
    float default_delta_time_ = 1.0f / 60.0f;

    // FPS 计算
    float fps_ = 0.0f;
    uint64_t fps_frame_count_ = 0;
    float fps_elapsed_ = 0.0f;
};
} // namespace mg

