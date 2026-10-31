#include "metrics.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <stdexcept>

double QualityEvaluator::compute_mse(const cv::Mat& ref, const cv::Mat& target) {
    if (ref.size() != target.size() || ref.type() != target.type()) {
        throw std::invalid_argument("Reference and target images must have identical dimensions and types for MSE.");
    }

    cv::Mat diff;
    cv::absdiff(ref, target, diff);
    diff.convertTo(diff, CV_64F);
    diff = diff.mul(diff);

    cv::Scalar s = cv::sum(diff);
    double total_sum = 0.0;
    for (int i = 0; i < ref.channels(); ++i) {
        total_sum += s[i];
    }

    double total_elements = static_cast<double>(ref.total() * ref.channels());
    return total_sum / total_elements;
}

double QualityEvaluator::compute_psnr(const cv::Mat& ref, const cv::Mat& target, double max_val) {
    double mse = compute_mse(ref, target);
    if (mse <= 1e-10) {
        return 100.0; // Identical images
    }
    return 10.0 * std::log10((max_val * max_val) / mse);
}

double QualityEvaluator::compute_ssim(const cv::Mat& ref, const cv::Mat& target) {
    if (ref.size() != target.size() || ref.type() != target.type()) {
        throw std::invalid_argument("Reference and target images must have identical dimensions and types for SSIM.");
    }

    const double C1 = 6.5025, C2 = 58.5225; // (0.01*255)^2, (0.03*255)^2

    cv::Mat I1, I2;
    ref.convertTo(I1, CV_64F);
    target.convertTo(I2, CV_64F);

    cv::Mat I1_2 = I1.mul(I1);
    cv::Mat I2_2 = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2 = mu1.mul(mu1);
    cv::Mat mu2_2 = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    cv::Mat t1 = 2.0 * mu1_mu2 + C1;
    cv::Mat t2 = 2.0 * sigma12 + C2;
    cv::Mat t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);

    cv::Mat ssim_map;
    cv::divide(t3, t1, ssim_map);

    cv::Scalar mssim = cv::mean(ssim_map);
    double ssim_val = 0.0;
    for (int i = 0; i < ref.channels(); ++i) {
        ssim_val += mssim[i];
    }
    return ssim_val / ref.channels();
}

QualityMetrics QualityEvaluator::evaluate(const cv::Mat& ref, const cv::Mat& target) {
    QualityMetrics q;
    try {
        q.mse = compute_mse(ref, target);
        q.psnr = compute_psnr(ref, target);
        q.ssim = compute_ssim(ref, target);
        q.valid = true;
    } catch (...) {
        q.valid = false;
    }
    return q;
}

