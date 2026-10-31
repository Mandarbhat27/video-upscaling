#pragma once

#include "interpolation.h"
#include "metrics.h"
#include <string>
#include <vector>

/**
 * @file benchmark.h
 * @brief Performance benchmarking harness.
 * Measures speedup, parallel efficiency, throughput (FPS),
 * latency variance, and visual quality across methods and thread counts.
 */

struct BenchmarkConfig {
    std::vector<int> thread_counts = {1, 2, 4, 8, 12};
    std::vector<ExecutionPolicy> policies = {ExecutionPolicy::OPENMP_STATIC, ExecutionPolicy::OPENMP_DYNAMIC, ExecutionPolicy::OPENMP_GUIDED};
    std::vector<InterpolationMethod> methods = {InterpolationMethod::NEAREST_NEIGHBOR, InterpolationMethod::BILINEAR, InterpolationMethod::BICUBIC};
    std::vector<double> scale_factors = {2.0, 4.0};
    int iterations = 10;
    int warmup_iterations = 2;
    int chunk_size = 16;
};

struct BenchmarkRecord {
    std::string method_name;
    std::string policy_name;
    int threads;
    int chunk_size;
    double scale_factor;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
    double avg_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    double stddev_latency_ms;
    double fps;
    double speedup;
    double parallel_efficiency;
    QualityMetrics quality;
};

class BenchmarkSuite {
public:
    BenchmarkSuite() = default;
    ~BenchmarkSuite() = default;

    /**
     * @brief Executes a comprehensive benchmark on a sample image/frame.
     */
    std::vector<BenchmarkRecord> run_benchmarks(const cv::Mat& test_frame, const BenchmarkConfig& config);

    /**
     * @brief Exports benchmark results to CSV format.
     */
    static bool export_csv(const std::string& filepath, const std::vector<BenchmarkRecord>& records);

    /**
     * @brief Exports benchmark results to JSON format.
     */
    static bool export_json(const std::string& filepath, const std::vector<BenchmarkRecord>& records);

private:
    FrameInterpolator interpolator_;
};

