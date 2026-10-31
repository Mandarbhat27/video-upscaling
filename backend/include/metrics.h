#pragma once

#include <opencv2/core.hpp>

/**
 * @file metrics.h
 * @brief Image quality evaluation metrics (PSNR, SSIM, MSE).
 * Used when a reference ground-truth image is available.
 */

struct QualityMetrics {
    double mse = 0.0;
    double psnr = 0.0;
    double ssim = 0.0;
    bool valid = false;
};

class QualityEvaluator {
public:
    static double compute_mse(const cv::Mat& ref, const cv::Mat& target);
    static double compute_psnr(const cv::Mat& ref, const cv::Mat& target, double max_val = 255.0);
    static double compute_ssim(const cv::Mat& ref, const cv::Mat& target);
    static QualityMetrics evaluate(const cv::Mat& ref, const cv::Mat& target);
};

