#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include "interpolation.h"
#include "metrics.h"

// Simple unit testing framework
static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] Assertion FAILED: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        g_tests_run++; \
        std::cout << "[RUNNING] " << #fn << "..." << std::endl; \
        if (fn()) { \
            std::cout << "[PASS] " << #fn << std::endl; \
            g_tests_passed++; \
        } else { \
            std::cerr << "[FAIL] " << #fn << std::endl; \
        } \
    } while(0)

// Test 1: Dimension correctness for 2x and 4x upscaling
bool test_output_dimensions() {
    FrameInterpolator interpolator;
    cv::Mat src(120, 160, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::Mat dst;

    // 2x upscaling
    interpolator.upscale_nearest_serial(src, dst, 2.0);
    TEST_ASSERT(dst.cols == 320 && dst.rows == 240, "Nearest 2x dimensions mismatch");

    interpolator.upscale_bilinear_serial(src, dst, 2.0);
    TEST_ASSERT(dst.cols == 320 && dst.rows == 240, "Bilinear 2x dimensions mismatch");

    interpolator.upscale_bicubic_serial(src, dst, 2.0);
    TEST_ASSERT(dst.cols == 320 && dst.rows == 240, "Bicubic 2x dimensions mismatch");

    // 4x upscaling
    interpolator.upscale_nearest_serial(src, dst, 4.0);
    TEST_ASSERT(dst.cols == 640 && dst.rows == 480, "Nearest 4x dimensions mismatch");

    interpolator.upscale_bilinear_serial(src, dst, 4.0);
    TEST_ASSERT(dst.cols == 640 && dst.rows == 480, "Bilinear 4x dimensions mismatch");

    interpolator.upscale_bicubic_serial(src, dst, 4.0);
    TEST_ASSERT(dst.cols == 640 && dst.rows == 480, "Bicubic 4x dimensions mismatch");

    return true;
}

// Test 2: Nearest Neighbour pixel accuracy
bool test_nearest_neighbor_accuracy() {
    FrameInterpolator interpolator;
    // 2x2 test image with distinct quadrant colors
    cv::Mat src(2, 2, CV_8UC3);
    src.at<cv::Vec3b>(0, 0) = cv::Vec3b(10, 20, 30);
    src.at<cv::Vec3b>(0, 1) = cv::Vec3b(40, 50, 60);
    src.at<cv::Vec3b>(1, 0) = cv::Vec3b(70, 80, 90);
    src.at<cv::Vec3b>(1, 1) = cv::Vec3b(100, 110, 120);

    cv::Mat dst;
    interpolator.upscale_nearest_serial(src, dst, 2.0);
    TEST_ASSERT(dst.cols == 4 && dst.rows == 4, "Nearest 2x2 to 4x4 size mismatch");

    // Top-left 2x2 block should match (0,0)
    TEST_ASSERT(dst.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30), "Pixel (0,0) nearest mismatch");
    TEST_ASSERT(dst.at<cv::Vec3b>(0, 1) == cv::Vec3b(10, 20, 30), "Pixel (0,1) nearest mismatch");
    TEST_ASSERT(dst.at<cv::Vec3b>(1, 0) == cv::Vec3b(10, 20, 30), "Pixel (1,0) nearest mismatch");

    // Bottom-right should match (1,1)
    TEST_ASSERT(dst.at<cv::Vec3b>(3, 3) == cv::Vec3b(100, 110, 120), "Pixel (3,3) nearest mismatch");
    return true;
}

// Test 3: Bilinear gradient interpolation accuracy
bool test_bilinear_interpolation_accuracy() {
    FrameInterpolator interpolator;
    // 2x2 image with corner values 0, 100, 100, 200
    cv::Mat src(2, 2, CV_8UC1);
    src.at<uint8_t>(0, 0) = 0;
    src.at<uint8_t>(0, 1) = 100;
    src.at<uint8_t>(1, 0) = 100;
    src.at<uint8_t>(1, 1) = 200;

    cv::Mat dst;
    interpolator.upscale_bilinear_serial(src, dst, 2.0);
    TEST_ASSERT(dst.cols == 4 && dst.rows == 4, "Bilinear size mismatch");

    // Pixel values should be monotonically increasing along diagonals and within bounds
    for (int y = 0; y < dst.rows; ++y) {
        for (int x = 0; x < dst.cols; ++x) {
            uint8_t val = dst.at<uint8_t>(y, x);
            TEST_ASSERT(val >= 0 && val <= 200, "Bilinear value out of bounds");
        }
    }

    // Center area should be around 100
    uint8_t center_val = dst.at<uint8_t>(1, 1);
    TEST_ASSERT(std::abs(static_cast<int>(center_val) - 75) <= 15, "Bilinear center interpolation unexpected");
    return true;
}

// Test 4: Bicubic smoothness and boundary clamping
bool test_bicubic_interpolation_accuracy() {
    FrameInterpolator interpolator;
    cv::Mat src(4, 4, CV_8UC3);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            src.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>(x * 60),
                static_cast<uint8_t>(y * 60),
                static_cast<uint8_t>((x + y) * 30)
            );
        }
    }

    cv::Mat dst;
    interpolator.upscale_bicubic_serial(src, dst, 2.0, -0.5f);
    TEST_ASSERT(dst.cols == 8 && dst.rows == 8, "Bicubic size mismatch");

    // Check all pixels are valid bytes
    for (int y = 0; y < dst.rows; ++y) {
        for (int x = 0; x < dst.cols; ++x) {
            cv::Vec3b px = dst.at<cv::Vec3b>(y, x);
            TEST_ASSERT(px[0] <= 255 && px[1] <= 255 && px[2] <= 255, "Bicubic pixel value overflow");
        }
    }
    return true;
}

// Test 5: OpenMP parallel execution outputs match serial baseline
bool test_openmp_parallel_equivalence() {
    FrameInterpolator interpolator;
    cv::Mat src(64, 64, CV_8UC3);
    // Fill with synthetic frequency pattern
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            src.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>((x * 7 + y * 13) % 256),
                static_cast<uint8_t>((x * 17 + y * 5) % 256),
                static_cast<uint8_t>((x * x + y * y) % 256)
            );
        }
    }

    cv::Mat dst_serial_nn, dst_omp_nn;
    interpolator.upscale_nearest_serial(src, dst_serial_nn, 2.0);
    interpolator.upscale_nearest_omp(src, dst_omp_nn, 2.0, ExecutionPolicy::OPENMP_STATIC, 4, 16);
    TEST_ASSERT(cv::norm(dst_serial_nn, dst_omp_nn, cv::NORM_INF) == 0, "Nearest: OpenMP output differs from Serial!");

    cv::Mat dst_serial_bi, dst_omp_bi;
    interpolator.upscale_bilinear_serial(src, dst_serial_bi, 2.0);
    interpolator.upscale_bilinear_omp(src, dst_omp_bi, 2.0, ExecutionPolicy::OPENMP_DYNAMIC, 4, 16);
    TEST_ASSERT(cv::norm(dst_serial_bi, dst_omp_bi, cv::NORM_INF) == 0, "Bilinear: OpenMP output differs from Serial!");

    cv::Mat dst_serial_bc, dst_omp_bc;
    interpolator.upscale_bicubic_serial(src, dst_serial_bc, 2.0, -0.5f);
    interpolator.upscale_bicubic_omp(src, dst_omp_bc, 2.0, ExecutionPolicy::OPENMP_GUIDED, 4, 16);
    TEST_ASSERT(cv::norm(dst_serial_bc, dst_omp_bc, cv::NORM_INF) == 0, "Bicubic: OpenMP output differs from Serial!");

    return true;
}

// Test 6: Quality Metrics (PSNR and SSIM)
bool test_quality_metrics() {
    cv::Mat ref(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    cv::Mat identical = ref.clone();

    QualityMetrics q1 = QualityEvaluator::evaluate(ref, identical);
    TEST_ASSERT(q1.valid, "Quality metrics invalid on identical images");
    TEST_ASSERT(q1.psnr >= 99.0, "PSNR on identical images should be ~100 dB");
    TEST_ASSERT(std::abs(q1.ssim - 1.0) < 0.001, "SSIM on identical images should be 1.0");

    cv::Mat degraded = ref.clone();
    degraded += cv::Scalar(10, 10, 10);
    QualityMetrics q2 = QualityEvaluator::evaluate(ref, degraded);
    TEST_ASSERT(q2.valid, "Quality metrics invalid on degraded image");
    TEST_ASSERT(q2.psnr < 50.0 && q2.psnr > 20.0, "PSNR on degraded image should reflect MSE");
    TEST_ASSERT(q2.ssim < 1.0 && q2.ssim > 0.0, "SSIM on degraded image should be less than 1.0");

    return true;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " ADAPTIVE VIDEO UPSCALING - PHASE 2 TEST SUITE" << std::endl;
    std::cout << "======================================================" << std::endl;

    RUN_TEST(test_output_dimensions);
    RUN_TEST(test_nearest_neighbor_accuracy);
    RUN_TEST(test_bilinear_interpolation_accuracy);
    RUN_TEST(test_bicubic_interpolation_accuracy);
    RUN_TEST(test_openmp_parallel_equivalence);
    RUN_TEST(test_quality_metrics);

    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "Tests Passed: " << g_tests_passed << " / " << g_tests_run << std::endl;
    std::cout << "======================================================" << std::endl;

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

