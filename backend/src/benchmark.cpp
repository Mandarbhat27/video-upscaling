#include "benchmark.h"
#include <fstream>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <iostream>

std::vector<BenchmarkRecord> BenchmarkSuite::run_benchmarks(const cv::Mat& test_frame, const BenchmarkConfig& config) {
    std::vector<BenchmarkRecord> records;
    if (test_frame.empty()) return records;

    cv::Mat dst;

    // For every scale factor
    for (double scale : config.scale_factors) {
        // Measure Serial Baseline for each method first to calculate speedups accurately
        std::vector<double> serial_baselines(3, 1.0);

        for (auto method : config.methods) {
            InterpolationConfig ser_cfg;
            ser_cfg.method = method;
            ser_cfg.policy = ExecutionPolicy::SERIAL;
            ser_cfg.num_threads = 1;

            // Warmup
            for (int w = 0; w < config.warmup_iterations; ++w) {
                interpolator_.upscale(test_frame, dst, scale, ser_cfg);
            }

            std::vector<double> latencies;
            for (int it = 0; it < config.iterations; ++it) {
                ExecutionMetrics m = interpolator_.upscale(test_frame, dst, scale, ser_cfg);
                latencies.push_back(m.execution_time_ms);
            }

            double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
            double avg_lat = sum / latencies.size();
            serial_baselines[static_cast<int>(method)] = avg_lat;

            BenchmarkRecord rec;
            rec.method_name = method_to_string(method);
            rec.policy_name = "SERIAL";
            rec.threads = 1;
            rec.chunk_size = 0;
            rec.scale_factor = scale;
            rec.src_width = test_frame.cols;
            rec.src_height = test_frame.rows;
            rec.dst_width = dst.cols;
            rec.dst_height = dst.rows;
            rec.avg_latency_ms = avg_lat;
            rec.min_latency_ms = *std::min_element(latencies.begin(), latencies.end());
            rec.max_latency_ms = *std::max_element(latencies.begin(), latencies.end());

            double sq_sum = 0.0;
            for (double l : latencies) sq_sum += (l - avg_lat) * (l - avg_lat);
            rec.stddev_latency_ms = std::sqrt(sq_sum / latencies.size());
            rec.fps = 1000.0 / avg_lat;
            rec.speedup = 1.0;
            rec.parallel_efficiency = 1.0;

            records.push_back(rec);
        }

        // Now test OpenMP Parallel Configurations
        for (auto method : config.methods) {
            double ser_lat = serial_baselines[static_cast<int>(method)];

            for (auto policy : config.policies) {
                for (int threads : config.thread_counts) {
                    InterpolationConfig omp_cfg;
                    omp_cfg.method = method;
                    omp_cfg.policy = policy;
                    omp_cfg.num_threads = threads;
                    omp_cfg.chunk_size = config.chunk_size;

                    // Warmup
                    for (int w = 0; w < config.warmup_iterations; ++w) {
                        interpolator_.upscale(test_frame, dst, scale, omp_cfg);
                    }

                    std::vector<double> latencies;
                    for (int it = 0; it < config.iterations; ++it) {
                        ExecutionMetrics m = interpolator_.upscale(test_frame, dst, scale, omp_cfg);
                        latencies.push_back(m.execution_time_ms);
                    }

                    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
                    double avg_lat = sum / latencies.size();

                    BenchmarkRecord rec;
                    rec.method_name = method_to_string(method);
                    rec.policy_name = policy_to_string(policy);
                    rec.threads = threads;
                    rec.chunk_size = config.chunk_size;
                    rec.scale_factor = scale;
                    rec.src_width = test_frame.cols;
                    rec.src_height = test_frame.rows;
                    rec.dst_width = dst.cols;
                    rec.dst_height = dst.rows;
                    rec.avg_latency_ms = avg_lat;
                    rec.min_latency_ms = *std::min_element(latencies.begin(), latencies.end());
                    rec.max_latency_ms = *std::max_element(latencies.begin(), latencies.end());

                    double sq_sum = 0.0;
                    for (double l : latencies) sq_sum += (l - avg_lat) * (l - avg_lat);
                    rec.stddev_latency_ms = std::sqrt(sq_sum / latencies.size());
                    rec.fps = 1000.0 / avg_lat;
                    rec.speedup = ser_lat / avg_lat;
                    rec.parallel_efficiency = rec.speedup / static_cast<double>(threads);

                    records.push_back(rec);
                }
            }
        }
    }

    return records;
}

bool BenchmarkSuite::export_csv(const std::string& filepath, const std::vector<BenchmarkRecord>& records) {
    std::ofstream file(filepath);
    if (!file.is_opened()) return false;

    file << "Method,Policy,Threads,ChunkSize,Scale,SrcWidth,SrcHeight,DstWidth,DstHeight,AvgLatencyMs,MinLatencyMs,MaxLatencyMs,StdDevMs,FPS,Speedup,ParallelEfficiency\n";
    for (const auto& r : records) {
        file << r.method_name << ","
             << r.policy_name << ","
             << r.threads << ","
             << r.chunk_size << ","
             << r.scale_factor << ","
             << r.src_width << ","
             << r.src_height << ","
             << r.dst_width << ","
             << r.dst_height << ","
             << std::fixed << std::setprecision(4)
             << r.avg_latency_ms << ","
             << r.min_latency_ms << ","
             << r.max_latency_ms << ","
             << r.stddev_latency_ms << ","
             << r.fps << ","
             << r.speedup << ","
             << r.parallel_efficiency << "\n";
    }
    file.close();
    return true;
}

bool BenchmarkSuite::export_json(const std::string& filepath, const std::vector<BenchmarkRecord>& records) {
    std::ofstream file(filepath);
    if (!file.is_opened()) return false;

    file << "[\n";
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        file << "  {\n"
             << "    \"method\": \"" << r.method_name << "\",\n"
             << "    \"policy\": \"" << r.policy_name << "\",\n"
             << "    \"threads\": " << r.threads << ",\n"
             << "    \"chunk_size\": " << r.chunk_size << ",\n"
             << "    \"scale_factor\": " << r.scale_factor << ",\n"
             << "    \"src_resolution\": \"" << r.src_width << "x" << r.src_height << "\",\n"
             << "    \"dst_resolution\": \"" << r.dst_width << "x" << r.dst_height << "\",\n"
             << "    \"avg_latency_ms\": " << r.avg_latency_ms << ",\n"
             << "    \"fps\": " << r.fps << ",\n"
             << "    \"speedup\": " << r.speedup << ",\n"
             << "    \"parallel_efficiency\": " << r.parallel_efficiency << "\n"
             << "  }" << (i + 1 < records.size() ? "," : "") << "\n";
    }
    file << "]\n";
    file.close();
    return true;
}

