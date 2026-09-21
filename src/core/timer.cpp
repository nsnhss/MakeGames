#include "timer.h"
#include <SDL.h>

namespace mg {

Timer::Timer()
{
    perf_freq_ = SDL_GetPerformanceFrequency();
    last_time_ = SDL_GetPerformanceCounter();
}

void Timer::Tick()
{
    uint64_t now = SDL_GetPerformanceCounter();
    double dt = (double)(now - last_time_) / (double)perf_freq_;
    last_time_ = now;

    // 限制最大 dt，防止调试断点或窗口拖拽导致 dt 爆炸（如 5 秒 → 物理/动画跳变）
    if (dt > (double)max_delta_time_)
        dt = (double)max_delta_time_;
    if (dt <= 0.0)
        dt = (double)default_delta_time_;

    delta_time_ = (float)dt;
    total_time_ += (float)dt;
    frame_count_++;

    // FPS 计算：基于最近 0.5 秒的帧数滑动窗口，避免逐帧波动过大
    fps_frame_count_++;
    fps_elapsed_ += (float)dt;
    if (fps_elapsed_ >= 0.5f) {
        fps_ = (float)fps_frame_count_ / fps_elapsed_;
        fps_frame_count_ = 0;
        fps_elapsed_ = 0.0f;
    }
}
} // namespace mg

