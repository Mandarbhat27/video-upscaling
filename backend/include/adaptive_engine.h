

#pragma once

#include "interpolation.h"
#include <string>
#include <vector>
#include <deque>
#include <chrono>

/**
 * @file adaptive_engine.h
 * @brief Feedback-driven real-time quality adaptation engine.
 * 
 * Monitors frame-processing performance against a latency budget
 * (derived from target FPS) and deterministically adjusts interpolation
 * quality using rolling windows, hysteresis thresholds, and cooldowns.
 */

enum class WorkloadMode {
    NORMAL = 1,
    INCREASED = 2,
    HIGH = 4
};

struct AdaptiveDecision {
    int frame_number = 0;
    InterpolationMethod previous_method = InterpolationMethod::BILINEAR;
    InterpolationMethod new_method = InterpolationMethod::BILINEAR;
    double measured_latency_ms = 0.0;
    double rolling_avg_latency_ms = 0.0;
    double target_budget_ms = 33.33;
    std::string reason;
    double timestamp_sec = 0.0;
};

struct AdaptiveStatus {
    double current_fps = 0.0;
    double rolling_avg_fps = 0.0;
    double current_latency_ms = 0.0;
    double rolling_avg_latency_ms = 0.0;
    double target_budget_ms = 33.33;
    double target_fps = 30.0;
    InterpolationMethod current_method = InterpolationMethod::BILINEAR;
    int current_threads = 4;
    int algorithm_switches = 0;
    int dropped_frames = 0;
    std::string state_message = "Stable";
    bool is_overloaded = false;
    WorkloadMode workload_level = WorkloadMode::NORMAL;
};

class AdaptiveEngine {
public:
    AdaptiveEngine(double target_fps = 30.0, int window_size = 10, int cooldown_frames = 15);
    ~AdaptiveEngine() = default;

    void set_target_fps(double fps);
    double get_target_fps() const { return target_fps_; }
    double get_frame_budget_ms() const { return target_budget_ms_; }

    void set_workload_mode(WorkloadMode mode) { workload_mode_ = mode; }
    WorkloadMode get_workload_mode() const { return workload_mode_; }

    void set_manual_method(InterpolationMethod method);
    void set_adaptive_enabled(bool enabled) { adaptive_enabled_ = enabled; }
    bool is_adaptive_enabled() const { return adaptive_enabled_; }

    /**
     * @brief Updates engine with the latest measured frame latency.
     * Evaluates hysteresis thresholds and switches interpolation method if necessary.
     * @param frame_idx Current frame index.
     * @param frame_latency_ms Measured interpolation latency in milliseconds.
     * @return InterpolationMethod Method to use for the next frame.
     */
    InterpolationMethod update_and_evaluate(int frame_idx, double frame_latency_ms);

    void record_dropped_frame() { dropped_frames_++; }

    AdaptiveStatus get_status() const;
    const std::vector<AdaptiveDecision>& get_decision_history() const { return decision_history_; }

    void reset();

private:
    double target_fps_;
    double target_budget_ms_;
    int window_size_;
    int cooldown_frames_;
    int frames_since_last_switch_;

    bool adaptive_enabled_;
    InterpolationMethod current_method_;
    WorkloadMode workload_mode_;

    std::deque<double> latency_history_;
    std::vector<AdaptiveDecision> decision_history_;

    int algorithm_switches_;
    int dropped_frames_;
    int consecutive_overloads_;
    int consecutive_underloads_;
    double last_measured_latency_ms_;

    // Thresholds
    double overload_ratio_ = 1.05;   // Trigger downgrade if rolling avg > 105% of budget
    double upgrade_ratio_ = 0.65;    // Trigger upgrade if rolling avg < 65% of budget
};

