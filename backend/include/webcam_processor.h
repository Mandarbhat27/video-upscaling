#pragma once

#include "interpolation.h"
#include "adaptive_engine.h"
#include <opencv2/videoio.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <string>

/**
 * @file webcam_processor.h
 * @brief Low-latency multithreaded live webcam processing pipeline.
 * Uses a detached capture thread and a 1-slot latest-frame buffer
 * with automatic stale frame discarding to minimise end-to-end latency.
 */

struct WebcamFramePacket {
    cv::Mat raw_frame;
    cv::Mat upscaled_frame;
    int frame_index = 0;
    double capture_timestamp_ms = 0.0;
    double process_start_ms = 0.0;
    double process_end_ms = 0.0;
    double interpolation_latency_ms = 0.0;
    double end_to_end_latency_ms = 0.0;
    InterpolationMethod method_used = InterpolationMethod::BILINEAR;
    int threads_used = 4;
    bool is_valid = false;
};

class WebcamProcessor {
public:
    WebcamProcessor();
    ~WebcamProcessor();

    bool start(int camera_id = 0, int width = 640, int height = 480);
    void stop();
    bool is_running() const { return is_running_.load(); }

    void set_scale_factor(double scale) { scale_factor_ = scale; }
    void set_config(const InterpolationConfig& config);
    void set_target_fps(double fps);
    void set_adaptive_enabled(bool enabled);
    void set_workload_mode(WorkloadMode mode);

    /**
     * @brief Fetches the latest upscaled frame and performance packet.
     * Non-blocking or minimal wait to maintain high UI responsiveness.
     */
    bool get_latest_frame(WebcamFramePacket& packet);

    AdaptiveStatus get_adaptive_status() const;
    int get_discarded_frames_count() const { return discarded_frames_.load(); }

private:
    void capture_thread_func();
    void processing_thread_func();

    std::atomic<bool> is_running_{false};
    int camera_id_{0};
    double scale_factor_{2.0};
    InterpolationConfig current_config_;

    std::unique_ptr<cv::VideoCapture> cap_;
    FrameInterpolator interpolator_;
    AdaptiveEngine adaptive_engine_;

    // Thread synchronisation for low-latency pipeline
    std::thread capture_thread_;
    std::thread process_thread_;

    std::mutex capture_mutex_;
    cv::Mat latest_captured_frame_;
    double latest_capture_time_ms_{0.0};
    bool new_frame_available_{false};
    std::condition_variable capture_cv_;

    std::mutex output_mutex_;
    WebcamFramePacket latest_output_packet_;
    bool new_output_ready_{false};

    std::atomic<int> captured_frames_count_{0};
    std::atomic<int> processed_frames_count_{0};
    std::atomic<int> discarded_frames_{0};
};

