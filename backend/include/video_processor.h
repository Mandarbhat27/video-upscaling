#pragma once

#include "interpolation.h"
#include "adaptive_engine.h"
#include "metrics.h"
#include <string>
#include <functional>

/**
 * @file video_processor.h
 * @brief Offline / Uploaded video processing pipeline.
 * Extracts frames, upscales sequentially maintaining exact frame order,
 * encodes to output video file, and computes overall benchmark statistics.
 */

struct VideoProcessingProgress {
    int current_frame = 0;
    int total_frames = 0;
    double progress_pct = 0.0;
    double current_fps = 0.0;
    double average_latency_ms = 0.0;
    InterpolationMethod current_method = InterpolationMethod::BILINEAR;
    int switches_count = 0;
    bool is_complete = false;
    std::string error_message;
};

struct VideoBenchmarkResult {
    std::string input_path;
    std::string output_path;
    int total_frames = 0;
    int src_width = 0;
    int src_height = 0;
    int dst_width = 0;
    int dst_height = 0;
    double scale_factor = 2.0;
    double original_fps = 30.0;
    double total_execution_time_sec = 0.0;
    double average_latency_ms = 0.0;
    double achieved_fps = 0.0;
    double speedup = 1.0;
    double parallel_efficiency = 1.0;
    InterpolationMethod method = InterpolationMethod::BILINEAR;
    ExecutionPolicy policy = ExecutionPolicy::SERIAL;
    int threads_used = 1;
    bool adaptive = false;
    int algorithm_switches = 0;
    std::vector<AdaptiveDecision> decisions;
    QualityMetrics quality;
};

class VideoProcessor {
public:
    VideoProcessor();
    ~VideoProcessor() = default;

    /**
     * @brief Processes an uploaded video file.
     * @param input_path Path to the input video.
     * @param output_path Path where the upscaled video will be saved.
     * @param scale_factor Upscaling multiplier (e.g. 2.0 or 4.0).
     * @param config Interpolation and execution configuration.
     * @param adaptive Whether adaptive quality mode is enabled.
     * @param target_fps Target processing rate (for adaptive decision-making).
     * @param progress_cb Callback reporting frame-by-frame progress.
     * @return VideoBenchmarkResult Complete execution metrics.
     */
    VideoBenchmarkResult process_video(
        const std::string& input_path,
        const std::string& output_path,
        double scale_factor,
        const InterpolationConfig& config,
        bool adaptive,
        double target_fps,
        std::function<void(const VideoProcessingProgress&)> progress_cb = nullptr
    );

    const VideoProcessingProgress& get_current_progress() const { return current_progress_; }

private:
    FrameInterpolator interpolator_;
    VideoProcessingProgress current_progress_;
};

