#include "video_processor.h"
#include <opencv2/videoio.hpp>
#include <omp.h>
#include <iostream>

VideoProcessor::VideoProcessor() {}

VideoBenchmarkResult VideoProcessor::process_video(
    const std::string& input_path,
    const std::string& output_path,
    double scale_factor,
    const InterpolationConfig& config,
    bool adaptive,
    double target_fps,
    std::function<void(const VideoProcessingProgress&)> progress_cb
) {
    VideoBenchmarkResult res;
    res.input_path = input_path;
    res.output_path = output_path;
    res.scale_factor = scale_factor;
    res.method = config.method;
    res.policy = config.policy;
    res.threads_used = (config.policy == ExecutionPolicy::SERIAL) ? 1 : config.num_threads;
    res.adaptive = adaptive;

    cv::VideoCapture cap(input_path);
    if (!cap.isOpened()) {
        current_progress_.error_message = "Failed to open input video: " + input_path;
        current_progress_.is_complete = true;
        if (progress_cb) progress_cb(current_progress_);
        return res;
    }

    int src_w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int src_h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0.0 || std::isnan(fps)) fps = 30.0;

    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    res.src_width = src_w;
    res.src_height = src_h;
    res.dst_width = dst_w;
    res.dst_height = dst_h;
    res.total_frames = total_frames;
    res.original_fps = fps;

    // Output writer with standard MP4 codec
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(output_path, fourcc, fps, cv::Size(dst_w, dst_h), true);
    if (!writer.isOpened()) {
        // Fallback codec
        fourcc = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
        writer.open(output_path, fourcc, fps, cv::Size(dst_w, dst_h), true);
    }

    AdaptiveEngine adaptive_engine(target_fps > 0.0 ? target_fps : fps);
    adaptive_engine.set_adaptive_enabled(adaptive);
    adaptive_engine.set_manual_method(config.method);

    cv::Mat frame, upscaled_frame;
    double total_interp_time_ms = 0.0;
    int processed_count = 0;

    double t_start_all = omp_get_wtime();

    InterpolationConfig current_cfg = config;

    while (cap.read(frame)) {
        if (frame.empty()) break;
        processed_count++;

        // Select method based on adaptive engine
        if (adaptive) {
            current_cfg.method = adaptive_engine.get_status().current_method;
        }

        // Measure pure interpolation latency
        ExecutionMetrics m = interpolator_.upscale(frame, upscaled_frame, scale_factor, current_cfg);
        total_interp_time_ms += m.execution_time_ms;

        // Feed measurement back to adaptive engine
        if (adaptive) {
            adaptive_engine.update_and_evaluate(processed_count, m.execution_time_ms);
        }

        // Write frame to output video
        if (writer.isOpened()) {
            writer.write(upscaled_frame);
        }

        // Report progress
        current_progress_.current_frame = processed_count;
        current_progress_.total_frames = total_frames;
        current_progress_.progress_pct = (total_frames > 0) ? (static_cast<double>(processed_count) * 100.0 / total_frames) : 0.0;
        current_progress_.current_fps = m.fps;
        current_progress_.average_latency_ms = total_interp_time_ms / processed_count;
        current_progress_.current_method = current_cfg.method;
        current_progress_.switches_count = adaptive_engine.get_status().algorithm_switches;
        current_progress_.is_complete = false;

        if (progress_cb && (processed_count % 5 == 0 || processed_count == total_frames)) {
            progress_cb(current_progress_);
        }
    }

    double t_end_all = omp_get_wtime();
    cap.release();
    writer.release();

    res.total_execution_time_sec = t_end_all - t_start_all;
    res.total_frames = processed_count;
    res.average_latency_ms = (processed_count > 0) ? (total_interp_time_ms / processed_count) : 0.0;
    res.achieved_fps = (res.total_execution_time_sec > 0.0) ? (processed_count / res.total_execution_time_sec) : 0.0;
    res.algorithm_switches = adaptive_engine.get_status().algorithm_switches;
    res.decisions = adaptive_engine.get_decision_history();

    current_progress_.is_complete = true;
    if (progress_cb) progress_cb(current_progress_);

    return res;
}

