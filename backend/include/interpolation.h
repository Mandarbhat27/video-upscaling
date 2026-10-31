#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <chrono>

/**
 * @file interpolation.h
 * @brief High-performance image and video frame upscaling interpolation engine.
 * 
 * Supports Nearest Neighbour, Bilinear, and Bicubic (Catmull-Rom) interpolation
 * with both serial execution and OpenMP parallelisation policies (static, dynamic, guided).
 */

enum class InterpolationMethod {
    NEAREST_NEIGHBOR = 0,
    BILINEAR = 1,
    BICUBIC = 2
};

enum class ExecutionPolicy {
    SERIAL = 0,
    OPENMP_STATIC = 1,
    OPENMP_DYNAMIC = 2,
    OPENMP_GUIDED = 3
};

struct InterpolationConfig {
    InterpolationMethod method = InterpolationMethod::BILINEAR;
    ExecutionPolicy policy = ExecutionPolicy::SERIAL;
    int num_threads = 1;      ///< Thread count for OpenMP (1 = serial / single-thread)
    int chunk_size = 16;      ///< OpenMP loop scheduling chunk size
    float bicubic_a = -0.5f;  ///< Catmull-Rom cubic parameter (default -0.5)
};

struct ExecutionMetrics {
    double execution_time_ms = 0.0;
    double fps = 0.0;
    int src_width = 0;
    int src_height = 0;
    int dst_width = 0;
    int dst_height = 0;
    double scale_factor = 2.0;
    InterpolationMethod method = InterpolationMethod::BILINEAR;
    ExecutionPolicy policy = ExecutionPolicy::SERIAL;
    int threads_used = 1;
};

// Helper name conversions
std::string method_to_string(InterpolationMethod method);
InterpolationMethod string_to_method(const std::string& name);

std::string policy_to_string(ExecutionPolicy policy);
ExecutionPolicy string_to_policy(const std::string& name);

/**
 * Core Interpolator Class
 * Encapsulates precomputed lookup tables and execution methods.
 */
class FrameInterpolator {
public:
    FrameInterpolator() = default;
    ~FrameInterpolator() = default;

    /**
     * @brief Upscales a frame using the specified configuration.
     * @param src Input source frame (CV_8UC3 or CV_8UC1).
     * @param dst Output destination frame. Memory is reused if pre-allocated.
     * @param scale_factor Upscale factor (e.g. 2.0 or 4.0).
     * @param config Interpolation method and execution parameters.
     * @return ExecutionMetrics Execution time and operational metadata.
     */
    ExecutionMetrics upscale(const cv::Mat& src, cv::Mat& dst, double scale_factor, const InterpolationConfig& config);

    // --- Serial implementations (Phase 2 Baseline) ---
    void upscale_nearest_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor);
    void upscale_bilinear_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor);
    void upscale_bicubic_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor, float a = -0.5f);

    // --- OpenMP parallel implementations (Phase 3) ---
    void upscale_nearest_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor, ExecutionPolicy policy, int num_threads, int chunk_size);
    void upscale_bilinear_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor, ExecutionPolicy policy, int num_threads, int chunk_size);
    void upscale_bicubic_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor, ExecutionPolicy policy, int num_threads, int chunk_size, float a = -0.5f);

    // Bicubic 1D weight computation (Keys kernel)
    static inline float cubic_weight(float x, float a) {
        float ax = std::abs(x);
        if (ax <= 1.0f) {
            return ((a + 2.0f) * ax - (a + 3.0f)) * ax * ax + 1.0f;
        } else if (ax < 2.0f) {
            return ((a * ax - 5.0f * a) * ax + 8.0f * a) * ax - 4.0f * a;
        }
        return 0.0f;
    }

    static inline uint8_t clamp_to_byte(int val) {
        return static_cast<uint8_t>(val < 0 ? 0 : (val > 255 ? 255 : val));
    }

    static inline uint8_t clamp_to_byte(float val) {
        int ival = static_cast<int>(val + 0.5f);
        return static_cast<uint8_t>(ival < 0 ? 0 : (ival > 255 ? 255 : ival));
    }
};

