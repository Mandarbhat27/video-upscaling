#include "adaptive_engine.h"
#include <numeric>
#include <chrono>

AdaptiveEngine::AdaptiveEngine(double target_fps, int window_size, int cooldown_frames)
    : target_fps_(target_fps),
      window_size_(window_size),
      cooldown_frames_(cooldown_frames),
      frames_since_last_switch_(cooldown_frames),
      adaptive_enabled_(true),
      current_method_(InterpolationMethod::BICUBIC),
      workload_mode_(WorkloadMode::NORMAL),
      algorithm_switches_(0),
      dropped_frames_(0),
      consecutive_overloads_(0),
      consecutive_underloads_(0),
      last_measured_latency_ms_(0.0) {
    set_target_fps(target_fps);
}

void AdaptiveEngine::set_target_fps(double fps) {
    target_fps_ = (fps > 1.0) ? fps : 30.0;
    target_budget_ms_ = 1000.0 / target_fps_;
}

void AdaptiveEngine::set_manual_method(InterpolationMethod method) {
    current_method_ = method;
    frames_since_last_switch_ = 0;
}

void AdaptiveEngine::reset() {
    latency_history_.clear();
    decision_history_.clear();
    algorithm_switches_ = 0;
    dropped_frames_ = 0;
    consecutive_overloads_ = 0;
    consecutive_underloads_ = 0;
    frames_since_last_switch_ = cooldown_frames_;
    current_method_ = InterpolationMethod::BICUBIC;
}

InterpolationMethod AdaptiveEngine::update_and_evaluate(int frame_idx, double frame_latency_ms) {
    last_measured_latency_ms_ = frame_latency_ms;
    latency_history_.push_back(frame_latency_ms);
    if (static_cast<int>(latency_history_.size()) > window_size_) {
        latency_history_.pop_front();
    }

    frames_since_last_switch_++;

    if (!adaptive_enabled_ || latency_history_.size() < 3) {
        return current_method_;
    }

    double sum = std::accumulate(latency_history_.begin(), latency_history_.end(), 0.0);
    double rolling_avg = sum / latency_history_.size();

    // Check cooldown period
    if (frames_since_last_switch_ < cooldown_frames_) {
        return current_method_;
    }

    // Overload condition (latency exceeds target budget)
    if (rolling_avg > (target_budget_ms_ * overload_ratio_)) {
        consecutive_overloads_++;
        consecutive_underloads_ = 0;

        if (consecutive_overloads_ >= 2) {
            InterpolationMethod prev = current_method_;
            bool switched = false;
            std::string reason;

            if (current_method_ == InterpolationMethod::BICUBIC) {
                current_method_ = InterpolationMethod::BILINEAR;
                reason = "Overload detected: latency " + std::to_string(rolling_avg) + "ms > budget " + std::to_string(target_budget_ms_) + "ms. Downgrading Bicubic -> Bilinear.";
                switched = true;
            } else if (current_method_ == InterpolationMethod::BILINEAR) {
                current_method_ = InterpolationMethod::NEAREST_NEIGHBOR;
                reason = "Severe overload: latency " + std::to_string(rolling_avg) + "ms > budget " + std::to_string(target_budget_ms_) + "ms. Downgrading Bilinear -> Nearest Neighbour.";
                switched = true;
            }

            if (switched) {
                algorithm_switches_++;
                frames_since_last_switch_ = 0;
                consecutive_overloads_ = 0;

                AdaptiveDecision d;
                d.frame_number = frame_idx;
                d.previous_method = prev;
                d.new_method = current_method_;
                d.measured_latency_ms = frame_latency_ms;
                d.rolling_avg_latency_ms = rolling_avg;
                d.target_budget_ms = target_budget_ms_;
                d.reason = reason;
                d.timestamp_sec = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()) / 1000.0;
                decision_history_.push_back(d);
            }
        }
    } 
    // Underload condition (system has plenty of headroom, can upgrade quality)
    else if (rolling_avg < (target_budget_ms_ * upgrade_ratio_)) {
        consecutive_underloads_++;
        consecutive_overloads_ = 0;

        // Require higher hysteresis (5 consecutive underloads) to avoid rapid oscillation
        if (consecutive_underloads_ >= 5) {
            InterpolationMethod prev = current_method_;
            bool switched = false;
            std::string reason;

            if (current_method_ == InterpolationMethod::NEAREST_NEIGHBOR) {
                current_method_ = InterpolationMethod::BILINEAR;
                reason = "Headroom available: latency " + std::to_string(rolling_avg) + "ms << budget " + std::to_string(target_budget_ms_) + "ms. Upgrading Nearest -> Bilinear.";
                switched = true;
            } else if (current_method_ == InterpolationMethod::BILINEAR) {
                current_method_ = InterpolationMethod::BICUBIC;
                reason = "Sustained headroom: latency " + std::to_string(rolling_avg) + "ms << budget " + std::to_string(target_budget_ms_) + "ms. Upgrading Bilinear -> Bicubic.";
                switched = true;
            }

            if (switched) {
                algorithm_switches_++;
                frames_since_last_switch_ = 0;
                consecutive_underloads_ = 0;

                AdaptiveDecision d;
                d.frame_number = frame_idx;
                d.previous_method = prev;
                d.new_method = current_method_;
                d.measured_latency_ms = frame_latency_ms;
                d.rolling_avg_latency_ms = rolling_avg;
                d.target_budget_ms = target_budget_ms_;
                d.reason = reason;
                d.timestamp_sec = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()) / 1000.0;
                decision_history_.push_back(d);
            }
        }
    } else {
        consecutive_overloads_ = 0;
        consecutive_underloads_ = 0;
    }

    return current_method_;
}

AdaptiveStatus AdaptiveEngine::get_status() const {
    AdaptiveStatus s;
    s.target_fps = target_fps_;
    s.target_budget_ms = target_budget_ms_;
    s.current_method = current_method_;
    s.algorithm_switches = algorithm_switches_;
    s.dropped_frames = dropped_frames_;
    s.workload_level = workload_mode_;
    s.current_latency_ms = last_measured_latency_ms_;
    s.current_fps = (last_measured_latency_ms_ > 0.001) ? (1000.0 / last_measured_latency_ms_) : 0.0;

    if (!latency_history_.empty()) {
        double sum = std::accumulate(latency_history_.begin(), latency_history_.end(), 0.0);
        s.rolling_avg_latency_ms = sum / latency_history_.size();
        s.rolling_avg_fps = (s.rolling_avg_latency_ms > 0.001) ? (1000.0 / s.rolling_avg_latency_ms) : 0.0;
    } else {
        s.rolling_avg_latency_ms = 0.0;
        s.rolling_avg_fps = 0.0;
    }

    s.is_overloaded = (s.rolling_avg_latency_ms > target_budget_ms_);
    if (!adaptive_enabled_) {
        s.state_message = "Fixed Quality (" + method_to_string(current_method_) + ")";
    } else if (s.is_overloaded) {
        s.state_message = "Processing Overload";
    } else if (s.rolling_avg_latency_ms < target_budget_ms_ * upgrade_ratio_) {
        s.state_message = "Optimal Headroom";
    } else {
        s.state_message = "Meeting Target (Stable)";
    }

    return s;
}

