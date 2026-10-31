#include "webcam_processor.h"
#include <omp.h>
#include <chrono>
#include <iostream>

static inline double get_current_time_ms() {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count()) / 1000.0;
}

WebcamProcessor::WebcamProcessor()
    : adaptive_engine_(30.0, 10, 15) {
    current_config_.method = InterpolationMethod::BICUBIC;
    current_config_.policy = ExecutionPolicy::OPENMP_STATIC;
    current_config_.num_threads = 4;
    current_config_.chunk_size = 16;
}

WebcamProcessor::~WebcamProcessor() {
    stop();
}

bool WebcamProcessor::start(int camera_id, int width, int height) {
    if (is_running_.load()) return true;

    camera_id_ = camera_id;
    cap_ = std::make_unique<cv::VideoCapture>(camera_id);
    if (!cap_->isOpened()) {
        std::cerr << "Failed to open webcam index " << camera_id << std::endl;
        return false;
    }

    if (width > 0 && height > 0) {
        cap_->set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap_->set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }

    is_running_.store(true);
    captured_frames_count_.store(0);
    processed_frames_count_.store(0);
    discarded_frames_.store(0);
    new_frame_available_ = false;
    new_output_ready_ = false;

    capture_thread_ = std::thread(&WebcamProcessor::capture_thread_func, this);
    process_thread_ = std::thread(&WebcamProcessor::processing_thread_func, this);

    return true;
}

void WebcamProcessor::stop() {
    if (!is_running_.load()) return;
    is_running_.store(false);

    capture_cv_.notify_all();

    if (capture_thread_.joinable()) capture_thread_.join();
    if (process_thread_.joinable()) process_thread_.join();

    if (cap_ && cap_->isOpened()) {
        cap_->release();
    }
}

void WebcamProcessor::set_config(const InterpolationConfig& config) {
    current_config_ = config;
}

void WebcamProcessor::set_target_fps(double fps) {
    adaptive_engine_.set_target_fps(fps);
}

void WebcamProcessor::set_adaptive_enabled(bool enabled) {
    adaptive_engine_.set_adaptive_enabled(enabled);
}

void WebcamProcessor::set_workload_mode(WorkloadMode mode) {
    adaptive_engine_.set_workload_mode(mode);
}

AdaptiveStatus WebcamProcessor::get_adaptive_status() const {
    AdaptiveStatus s = adaptive_engine_.get_status();
    s.dropped_frames = discarded_frames_.load();
    s.current_threads = (current_config_.policy == ExecutionPolicy::SERIAL) ? 1 : current_config_.num_threads;
    return s;
}

void WebcamProcessor::capture_thread_func() {
    cv::Mat grabbed_frame;
    while (is_running_.load()) {
        if (!cap_->read(grabbed_frame) || grabbed_frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        double capture_time = get_current_time_ms();
        captured_frames_count_++;

        {
            std::lock_guard<std::mutex> lock(capture_mutex_);
            // Stale frame handling: if previous captured frame was not yet processed, drop it
            if (new_frame_available_) {
                discarded_frames_++;
            }
            grabbed_frame.copyTo(latest_captured_frame_);
            latest_capture_time_ms_ = capture_time;
            new_frame_available_ = true;
        }
        capture_cv_.notify_one();
    }
}

void WebcamProcessor::processing_thread_func() {
    cv::Mat frame_to_process;
    cv::Mat upscaled_buffer;
    double capture_time = 0.0;

    while (is_running_.load()) {
        {
            std::unique_lock<std::mutex> lock(capture_mutex_);
            capture_cv_.wait(lock, [this] {
                return new_frame_available_ || !is_running_.load();
            });

            if (!is_running_.load()) break;

            latest_captured_frame_.copyTo(frame_to_process);
            capture_time = latest_capture_time_ms_;
            new_frame_available_ = false; // Consumed
        }

        if (frame_to_process.empty()) continue;

        double t_proc_start = get_current_time_ms();
        int frame_idx = processed_frames_count_.fetch_add(1) + 1;

        InterpolationConfig active_cfg = current_config_;
        if (adaptive_engine_.is_adaptive_enabled()) {
            active_cfg.method = adaptive_engine_.get_status().current_method;
        }

        // Section 10: Controlled Workload Stress
        // If increased or high workload mode is selected, genuinely perform extra passes
        WorkloadMode mode = adaptive_engine_.get_workload_mode();
        int workload_multiplier = static_cast<int>(mode);

        for (int w = 0; w < workload_multiplier; ++w) {
            interpolator_.upscale(frame_to_process, upscaled_buffer, scale_factor_, active_cfg);
        }

        double t_proc_end = get_current_time_ms();
        double interp_latency = t_proc_end - t_proc_start;
        double end_to_end_latency = t_proc_end - capture_time;

        if (adaptive_engine_.is_adaptive_enabled()) {
            adaptive_engine_.update_and_evaluate(frame_idx, interp_latency);
        }

        {
            std::lock_guard<std::mutex> lock(output_mutex_);
            latest_output_packet_.raw_frame = frame_to_process;
            latest_output_packet_.upscaled_frame = upscaled_buffer;
            latest_output_packet_.frame_index = frame_idx;
            latest_output_packet_.capture_timestamp_ms = capture_time;
            latest_output_packet_.process_start_ms = t_proc_start;
            latest_output_packet_.process_end_ms = t_proc_end;
            latest_output_packet_.interpolation_latency_ms = interp_latency;
            latest_output_packet_.end_to_end_latency_ms = end_to_end_latency;
            latest_output_packet_.method_used = active_cfg.method;
            latest_output_packet_.threads_used = (active_cfg.policy == ExecutionPolicy::SERIAL) ? 1 : active_cfg.num_threads;
            latest_output_packet_.is_valid = true;
            new_output_ready_ = true;
        }
    }
}

bool WebcamProcessor::get_latest_frame(WebcamFramePacket& packet) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (!latest_output_packet_.is_valid) {
        return false;
    }
    packet = latest_output_packet_;
    return true;
}

