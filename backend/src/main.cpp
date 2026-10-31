#include <iostream>
#include <string>
#include <vector>
#include "interpolation.h"
#include "video_processor.h"
#include "webcam_processor.h"
#include "benchmark.h"

void print_usage(const char* prog) {
    std::cout << "Adaptive Video Upscaling CLI Engine\n"
              << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --benchmark [image_path] [out_csv]  Run benchmark suite on an image/frame\n"
              << "  --video <input> <output>            Process an uploaded video\n"
              << "  --scale <factor>                    Upscale multiplier: 2.0 or 4.0 (default 2.0)\n"
              << "  --method <nearest|bilinear|bicubic> Select interpolation algorithm (default bilinear)\n"
              << "  --policy <serial|static|dynamic|guided> OpenMP scheduling policy (default static)\n"
              << "  --threads <n>                       Number of OpenMP threads (default 4)\n"
              << "  --adaptive                          Enable real-time adaptive quality selection\n"
              << "  --target-fps <fps>                  Target frame rate for adaptive budget (default 30)\n"
              << "  --help                              Display this help message\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    std::string mode = argv[1];

    if (mode == "--help" || mode == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    if (mode == "--benchmark") {
        std::string test_img = (argc > 2) ? argv[2] : "";
        std::string out_csv = (argc > 3) ? argv[3] : "results/benchmark_results.csv";
        std::string out_json = "results/benchmark_results.json";

        cv::Mat frame;
        if (!test_img.empty()) {
            frame = cv::imread(test_img);
        }
        if (frame.empty()) {
            std::cout << "[INFO] No input image provided or failed to load. Generating 640x360 synthetic test frame..." << std::endl;
            frame = cv::Mat(360, 640, CV_8UC3);
            for (int y = 0; y < 360; ++y) {
                for (int x = 0; x < 640; ++x) {
                    frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uint8_t>((x * 255) / 640),
                        static_cast<uint8_t>((y * 255) / 360),
                        static_cast<uint8_t>(((x + y) * 255) / 1000)
                    );
                }
            }
        }

        std::cout << "[INFO] Running benchmark suite on " << frame.cols << "x" << frame.rows << " frame..." << std::endl;
        BenchmarkSuite suite;
        BenchmarkConfig cfg;
        cfg.thread_counts = {1, 2, 4, 8, 12};
        cfg.scale_factors = {2.0, 4.0};
        cfg.iterations = 5;
        cfg.warmup_iterations = 2;

        auto records = suite.run_benchmarks(frame, cfg);
        suite.export_csv(out_csv, records);
        suite.export_json(out_json, records);

        std::cout << "[SUCCESS] Benchmark completed! Exported " << records.size() 
                  << " records to " << out_csv << " and " << out_json << std::endl;
        return 0;
    }

    if (mode == "--video") {
        if (argc < 4) {
            std::cerr << "Error: --video requires <input_path> and <output_path>" << std::endl;
            return 1;
        }
        std::string input_path = argv[2];
        std::string output_path = argv[3];

        double scale = 2.0;
        InterpolationConfig config;
        config.method = InterpolationMethod::BILINEAR;
        config.policy = ExecutionPolicy::OPENMP_STATIC;
        config.num_threads = 4;
        bool adaptive = false;
        double target_fps = 30.0;

        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--scale" && i + 1 < argc) scale = std::stod(argv[++i]);
            else if (arg == "--method" && i + 1 < argc) config.method = string_to_method(argv[++i]);
            else if (arg == "--policy" && i + 1 < argc) config.policy = string_to_policy(argv[++i]);
            else if (arg == "--threads" && i + 1 < argc) config.num_threads = std::stoi(argv[++i]);
            else if (arg == "--adaptive") adaptive = true;
            else if (arg == "--target-fps" && i + 1 < argc) target_fps = std::stod(argv[++i]);
        }

        VideoProcessor vp;
        std::cout << "[INFO] Starting video processing: " << input_path << " -> " << output_path << std::endl;
        auto res = vp.process_video(input_path, output_path, scale, config, adaptive, target_fps,
            [](const VideoProcessingProgress& p) {
                std::cout << "\r[PROGRESS] Frame " << p.current_frame << "/" << p.total_frames
                          << " (" << static_cast<int>(p.progress_pct) << "%) | FPS: " << p.current_fps
                          << " | Method: " << method_to_string(p.current_method) << std::flush;
            }
        );
        std::cout << "\n[SUCCESS] Completed in " << res.total_execution_time_sec << "s | Avg Latency: " 
                  << res.average_latency_ms << "ms | Achieved FPS: " << res.achieved_fps << std::endl;
        return 0;
    }

    print_usage(argv[0]);
    return 0;
}
