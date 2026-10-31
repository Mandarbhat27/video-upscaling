#include "interpolation.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <omp.h>

std::string method_to_string(InterpolationMethod method) {
    switch (method) {
        case InterpolationMethod::NEAREST_NEIGHBOR: return "NEAREST";
        case InterpolationMethod::BILINEAR: return "BILINEAR";
        case InterpolationMethod::BICUBIC: return "BICUBIC";
        default: return "UNKNOWN";
    }
}

InterpolationMethod string_to_method(const std::string& name) {
    if (name == "NEAREST" || name == "nearest" || name == "0") return InterpolationMethod::NEAREST_NEIGHBOR;
    if (name == "BILINEAR" || name == "bilinear" || name == "1") return InterpolationMethod::BILINEAR;
    if (name == "BICUBIC" || name == "bicubic" || name == "2") return InterpolationMethod::BICUBIC;
    return InterpolationMethod::BILINEAR;
}

std::string policy_to_string(ExecutionPolicy policy) {
    switch (policy) {
        case ExecutionPolicy::SERIAL: return "SERIAL";
        case ExecutionPolicy::OPENMP_STATIC: return "OPENMP_STATIC";
        case ExecutionPolicy::OPENMP_DYNAMIC: return "OPENMP_DYNAMIC";
        case ExecutionPolicy::OPENMP_GUIDED: return "OPENMP_GUIDED";
        default: return "UNKNOWN";
    }
}

ExecutionPolicy string_to_policy(const std::string& name) {
    if (name == "SERIAL" || name == "serial") return ExecutionPolicy::SERIAL;
    if (name == "OPENMP_STATIC" || name == "static") return ExecutionPolicy::OPENMP_STATIC;
    if (name == "OPENMP_DYNAMIC" || name == "dynamic") return ExecutionPolicy::OPENMP_DYNAMIC;
    if (name == "OPENMP_GUIDED" || name == "guided") return ExecutionPolicy::OPENMP_GUIDED;
    return ExecutionPolicy::SERIAL;
}

// ============================================================================
// PHASE 2: SERIAL BASELINE IMPLEMENTATIONS
// ============================================================================

void FrameInterpolator::upscale_nearest_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    // Precompute horizontal source index mapping
    std::vector<int> map_x(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int sx = static_cast<int>(std::round(src_xf));
        map_x[x] = std::clamp(sx, 0, src_w - 1);
    }

    if (channels == 3) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);

            const uint8_t* src_row = src.ptr<uint8_t>(sy);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                int sx = map_x[x];
                int src_idx = sx * 3;
                int dst_idx = x * 3;
                dst_row[dst_idx]     = src_row[src_idx];
                dst_row[dst_idx + 1] = src_row[src_idx + 1];
                dst_row[dst_idx + 2] = src_row[src_idx + 2];
            }
        }
    } else if (channels == 1) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);

            const uint8_t* src_row = src.ptr<uint8_t>(sy);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                dst_row[x] = src_row[map_x[x]];
            }
        }
    }
}

void FrameInterpolator::upscale_bilinear_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    // Precompute 1D horizontal sampling coordinates and weights
    struct Sample1D {
        int idx0;
        int idx1;
        float w0;
        float w1;
    };

    std::vector<Sample1D> x_samples(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int x0 = static_cast<int>(std::floor(src_xf));
        int x1 = x0 + 1;
        float u = src_xf - static_cast<float>(x0);

        x_samples[x].idx0 = std::clamp(x0, 0, src_w - 1);
        x_samples[x].idx1 = std::clamp(x1, 0, src_w - 1);
        x_samples[x].w1 = u;
        x_samples[x].w0 = 1.0f - u;
    }

    if (channels == 3) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            int y1 = y0 + 1;
            float v = src_yf - static_cast<float>(y0);
            float vy0 = 1.0f - v;
            float vy1 = v;

            int sy0 = std::clamp(y0, 0, src_h - 1);
            int sy1 = std::clamp(y1, 0, src_h - 1);

            const uint8_t* row0 = src.ptr<uint8_t>(sy0);
            const uint8_t* row1 = src.ptr<uint8_t>(sy1);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                const Sample1D& sx = x_samples[x];

                int p00 = sx.idx0 * 3;
                int p10 = sx.idx1 * 3;

                float w00 = vy0 * sx.w0;
                float w10 = vy0 * sx.w1;
                float w01 = vy1 * sx.w0;
                float w11 = vy1 * sx.w1;

                int dst_idx = x * 3;
                // Channel 0 (Blue)
                float b = w00 * row0[p00]     + w10 * row0[p10]     + w01 * row1[p00]     + w11 * row1[p10];
                // Channel 1 (Green)
                float g = w00 * row0[p00 + 1] + w10 * row0[p10 + 1] + w01 * row1[p00 + 1] + w11 * row1[p10 + 1];
                // Channel 2 (Red)
                float r = w00 * row0[p00 + 2] + w10 * row0[p10 + 2] + w01 * row1[p00 + 2] + w11 * row1[p10 + 2];

                dst_row[dst_idx]     = clamp_to_byte(b);
                dst_row[dst_idx + 1] = clamp_to_byte(g);
                dst_row[dst_idx + 2] = clamp_to_byte(r);
            }
        }
    } else if (channels == 1) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            int y1 = y0 + 1;
            float v = src_yf - static_cast<float>(y0);
            float vy0 = 1.0f - v;
            float vy1 = v;

            int sy0 = std::clamp(y0, 0, src_h - 1);
            int sy1 = std::clamp(y1, 0, src_h - 1);

            const uint8_t* row0 = src.ptr<uint8_t>(sy0);
            const uint8_t* row1 = src.ptr<uint8_t>(sy1);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                const Sample1D& sx = x_samples[x];
                float val = vy0 * (sx.w0 * row0[sx.idx0] + sx.w1 * row0[sx.idx1]) +
                            vy1 * (sx.w0 * row1[sx.idx0] + sx.w1 * row1[sx.idx1]);
                dst_row[x] = clamp_to_byte(val);
            }
        }
    }
}

void FrameInterpolator::upscale_bicubic_serial(const cv::Mat& src, cv::Mat& dst, double scale_factor, float a) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    // Precompute 1D horizontal cubic coordinate indices and weights
    struct CubicSample1D {
        int idx[4];
        float w[4];
    };

    std::vector<CubicSample1D> x_samples(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int x0 = static_cast<int>(std::floor(src_xf));
        float u = src_xf - static_cast<float>(x0);

        float sum = 0.0f;
        for (int m = 0; m < 4; ++m) {
            int sx = x0 + m - 1;
            x_samples[x].idx[m] = std::clamp(sx, 0, src_w - 1);
            float weight = cubic_weight(static_cast<float>(m - 1) - u, a);
            x_samples[x].w[m] = weight;
            sum += weight;
        }
        // Normalize weights to guard against minor floating point drift
        if (std::abs(sum) > 1e-6f) {
            float inv_sum = 1.0f / sum;
            for (int m = 0; m < 4; ++m) {
                x_samples[x].w[m] *= inv_sum;
            }
        }
    }

    if (channels == 3) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            float v = src_yf - static_cast<float>(y0);

            int y_idx[4];
            float wy[4];
            float sum_y = 0.0f;
            for (int n = 0; n < 4; ++n) {
                y_idx[n] = std::clamp(y0 + n - 1, 0, src_h - 1);
                wy[n] = cubic_weight(static_cast<float>(n - 1) - v, a);
                sum_y += wy[n];
            }
            if (std::abs(sum_y) > 1e-6f) {
                float inv_sum_y = 1.0f / sum_y;
                for (int n = 0; n < 4; ++n) wy[n] *= inv_sum_y;
            }

            const uint8_t* rows[4] = {
                src.ptr<uint8_t>(y_idx[0]),
                src.ptr<uint8_t>(y_idx[1]),
                src.ptr<uint8_t>(y_idx[2]),
                src.ptr<uint8_t>(y_idx[3])
            };
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                const CubicSample1D& sx = x_samples[x];

                float b = 0.0f, g = 0.0f, r = 0.0f;
                for (int n = 0; n < 4; ++n) {
                    float row_b = 0.0f, row_g = 0.0f, row_r = 0.0f;
                    const uint8_t* r_ptr = rows[n];
                    for (int m = 0; m < 4; ++m) {
                        int p = sx.idx[m] * 3;
                        float wm = sx.w[m];
                        row_b += wm * r_ptr[p];
                        row_g += wm * r_ptr[p + 1];
                        row_r += wm * r_ptr[p + 2];
                    }
                    b += wy[n] * row_b;
                    g += wy[n] * row_g;
                    r += wy[n] * row_r;
                }

                int dst_idx = x * 3;
                dst_row[dst_idx]     = clamp_to_byte(b);
                dst_row[dst_idx + 1] = clamp_to_byte(g);
                dst_row[dst_idx + 2] = clamp_to_byte(r);
            }
        }
    } else if (channels == 1) {
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            float v = src_yf - static_cast<float>(y0);

            int y_idx[4];
            float wy[4];
            float sum_y = 0.0f;
            for (int n = 0; n < 4; ++n) {
                y_idx[n] = std::clamp(y0 + n - 1, 0, src_h - 1);
                wy[n] = cubic_weight(static_cast<float>(n - 1) - v, a);
                sum_y += wy[n];
            }
            if (std::abs(sum_y) > 1e-6f) {
                float inv_sum_y = 1.0f / sum_y;
                for (int n = 0; n < 4; ++n) wy[n] *= inv_sum_y;
            }

            const uint8_t* rows[4] = {
                src.ptr<uint8_t>(y_idx[0]),
                src.ptr<uint8_t>(y_idx[1]),
                src.ptr<uint8_t>(y_idx[2]),
                src.ptr<uint8_t>(y_idx[3])
            };
            uint8_t* dst_row = dst.ptr<uint8_t>(y);

            for (int x = 0; x < dst_w; ++x) {
                const CubicSample1D& sx = x_samples[x];
                float val = 0.0f;
                for (int n = 0; n < 4; ++n) {
                    float row_val = 0.0f;
                    const uint8_t* r_ptr = rows[n];
                    for (int m = 0; m < 4; ++m) {
                        row_val += sx.w[m] * r_ptr[sx.idx[m]];
                    }
                    val += wy[n] * row_val;
                }
                dst_row[x] = clamp_to_byte(val);
            }
        }
    }
}

// ============================================================================
// PHASE 3: OPENMP PARALLEL PROCESSING IMPLEMENTATIONS
// ============================================================================

void FrameInterpolator::upscale_nearest_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor,
                                           ExecutionPolicy policy, int num_threads, int chunk_size) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    std::vector<int> map_x(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        map_x[x] = std::clamp(static_cast<int>(std::round(src_xf)), 0, src_w - 1);
    }

    if (num_threads <= 0) num_threads = omp_get_max_threads();
    if (chunk_size <= 0) chunk_size = std::max(1, dst_h / (num_threads * 4));

    if (policy == ExecutionPolicy::OPENMP_DYNAMIC) {
        #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);
            const uint8_t* src_row = src.ptr<uint8_t>(sy);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);
            if (channels == 3) {
                for (int x = 0; x < dst_w; ++x) {
                    int p_src = map_x[x] * 3;
                    int p_dst = x * 3;
                    dst_row[p_dst]     = src_row[p_src];
                    dst_row[p_dst + 1] = src_row[p_src + 1];
                    dst_row[p_dst + 2] = src_row[p_src + 2];
                }
            } else {
                for (int x = 0; x < dst_w; ++x) dst_row[x] = src_row[map_x[x]];
            }
        }
    } else if (policy == ExecutionPolicy::OPENMP_GUIDED) {
        #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);
            const uint8_t* src_row = src.ptr<uint8_t>(sy);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);
            if (channels == 3) {
                for (int x = 0; x < dst_w; ++x) {
                    int p_src = map_x[x] * 3;
                    int p_dst = x * 3;
                    dst_row[p_dst]     = src_row[p_src];
                    dst_row[p_dst + 1] = src_row[p_src + 1];
                    dst_row[p_dst + 2] = src_row[p_src + 2];
                }
            } else {
                for (int x = 0; x < dst_w; ++x) dst_row[x] = src_row[map_x[x]];
            }
        }
    } else { // OPENMP_STATIC or default
        #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
        for (int y = 0; y < dst_h; ++y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
            int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);
            const uint8_t* src_row = src.ptr<uint8_t>(sy);
            uint8_t* dst_row = dst.ptr<uint8_t>(y);
            if (channels == 3) {
                for (int x = 0; x < dst_w; ++x) {
                    int p_src = map_x[x] * 3;
                    int p_dst = x * 3;
                    dst_row[p_dst]     = src_row[p_src];
                    dst_row[p_dst + 1] = src_row[p_src + 1];
                    dst_row[p_dst + 2] = src_row[p_src + 2];
                }
            } else {
                for (int x = 0; x < dst_w; ++x) dst_row[x] = src_row[map_x[x]];
            }
        }
    }
}

void FrameInterpolator::upscale_bilinear_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor,
                                            ExecutionPolicy policy, int num_threads, int chunk_size) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    struct Sample1D {
        int idx0;
        int idx1;
        float w0;
        float w1;
    };

    std::vector<Sample1D> x_samples(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int x0 = static_cast<int>(std::floor(src_xf));
        int x1 = x0 + 1;
        float u = src_xf - static_cast<float>(x0);

        x_samples[x].idx0 = std::clamp(x0, 0, src_w - 1);
        x_samples[x].idx1 = std::clamp(x1, 0, src_w - 1);
        x_samples[x].w1 = u;
        x_samples[x].w0 = 1.0f - u;
    }

    if (num_threads <= 0) num_threads = omp_get_max_threads();
    if (chunk_size <= 0) chunk_size = std::max(1, dst_h / (num_threads * 4));

    auto process_row = [&](int y) {
        float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int y0 = static_cast<int>(std::floor(src_yf));
        int y1 = y0 + 1;
        float v = src_yf - static_cast<float>(y0);
        float vy0 = 1.0f - v;
        float vy1 = v;

        int sy0 = std::clamp(y0, 0, src_h - 1);
        int sy1 = std::clamp(y1, 0, src_h - 1);

        const uint8_t* row0 = src.ptr<uint8_t>(sy0);
        const uint8_t* row1 = src.ptr<uint8_t>(sy1);
        uint8_t* dst_row = dst.ptr<uint8_t>(y);

        if (channels == 3) {
            for (int x = 0; x < dst_w; ++x) {
                const Sample1D& sx = x_samples[x];
                int p00 = sx.idx0 * 3;
                int p10 = sx.idx1 * 3;

                float w00 = vy0 * sx.w0;
                float w10 = vy0 * sx.w1;
                float w01 = vy1 * sx.w0;
                float w11 = vy1 * sx.w1;

                int dst_idx = x * 3;
                float b = w00 * row0[p00]     + w10 * row0[p10]     + w01 * row1[p00]     + w11 * row1[p10];
                float g = w00 * row0[p00 + 1] + w10 * row0[p10 + 1] + w01 * row1[p00 + 1] + w11 * row1[p10 + 1];
                float r = w00 * row0[p00 + 2] + w10 * row0[p10 + 2] + w01 * row1[p00 + 2] + w11 * row1[p10 + 2];

                dst_row[dst_idx]     = clamp_to_byte(b);
                dst_row[dst_idx + 1] = clamp_to_byte(g);
                dst_row[dst_idx + 2] = clamp_to_byte(r);
            }
        } else {
            for (int x = 0; x < dst_w; ++x) {
                const Sample1D& sx = x_samples[x];
                float val = vy0 * (sx.w0 * row0[sx.idx0] + sx.w1 * row0[sx.idx1]) +
                            vy1 * (sx.w0 * row1[sx.idx0] + sx.w1 * row1[sx.idx1]);
                dst_row[x] = clamp_to_byte(val);
            }
        }
    };

    if (policy == ExecutionPolicy::OPENMP_DYNAMIC) {
        #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    } else if (policy == ExecutionPolicy::OPENMP_GUIDED) {
        #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    } else { // OPENMP_STATIC or default
        #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    }
}

void FrameInterpolator::upscale_bicubic_omp(const cv::Mat& src, cv::Mat& dst, double scale_factor,
                                           ExecutionPolicy policy, int num_threads, int chunk_size, float a) {
    int src_w = src.cols;
    int src_h = src.rows;
    int channels = src.channels();
    int dst_w = static_cast<int>(std::round(src_w * scale_factor));
    int dst_h = static_cast<int>(std::round(src_h * scale_factor));

    dst.create(dst_h, dst_w, src.type());

    struct CubicSample1D {
        int idx[4];
        float w[4];
    };

    std::vector<CubicSample1D> x_samples(dst_w);
    for (int x = 0; x < dst_w; ++x) {
        float src_xf = (static_cast<float>(x) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int x0 = static_cast<int>(std::floor(src_xf));
        float u = src_xf - static_cast<float>(x0);

        float sum = 0.0f;
        for (int m = 0; m < 4; ++m) {
            int sx = x0 + m - 1;
            x_samples[x].idx[m] = std::clamp(sx, 0, src_w - 1);
            float weight = cubic_weight(static_cast<float>(m - 1) - u, a);
            x_samples[x].w[m] = weight;
            sum += weight;
        }
        if (std::abs(sum) > 1e-6f) {
            float inv_sum = 1.0f / sum;
            for (int m = 0; m < 4; ++m) x_samples[x].w[m] *= inv_sum;
        }
    }

    if (num_threads <= 0) num_threads = omp_get_max_threads();
    if (chunk_size <= 0) chunk_size = std::max(1, dst_h / (num_threads * 4));

    auto process_row = [&](int y) {
        float src_yf = (static_cast<float>(y) + 0.5f) / static_cast<float>(scale_factor) - 0.5f;
        int y0 = static_cast<int>(std::floor(src_yf));
        float v = src_yf - static_cast<float>(y0);

        int y_idx[4];
        float wy[4];
        float sum_y = 0.0f;
        for (int n = 0; n < 4; ++n) {
            y_idx[n] = std::clamp(y0 + n - 1, 0, src_h - 1);
            wy[n] = cubic_weight(static_cast<float>(n - 1) - v, a);
            sum_y += wy[n];
        }
        if (std::abs(sum_y) > 1e-6f) {
            float inv_sum_y = 1.0f / sum_y;
            for (int n = 0; n < 4; ++n) wy[n] *= inv_sum_y;
        }

        const uint8_t* rows[4] = {
            src.ptr<uint8_t>(y_idx[0]),
            src.ptr<uint8_t>(y_idx[1]),
            src.ptr<uint8_t>(y_idx[2]),
            src.ptr<uint8_t>(y_idx[3])
        };
        uint8_t* dst_row = dst.ptr<uint8_t>(y);

        if (channels == 3) {
            for (int x = 0; x < dst_w; ++x) {
                const CubicSample1D& sx = x_samples[x];
                float b = 0.0f, g = 0.0f, r = 0.0f;
                for (int n = 0; n < 4; ++n) {
                    float row_b = 0.0f, row_g = 0.0f, row_r = 0.0f;
                    const uint8_t* r_ptr = rows[n];
                    for (int m = 0; m < 4; ++m) {
                        int p = sx.idx[m] * 3;
                        float wm = sx.w[m];
                        row_b += wm * r_ptr[p];
                        row_g += wm * r_ptr[p + 1];
                        row_r += wm * r_ptr[p + 2];
                    }
                    b += wy[n] * row_b;
                    g += wy[n] * row_g;
                    r += wy[n] * row_r;
                }

                int dst_idx = x * 3;
                dst_row[dst_idx]     = clamp_to_byte(b);
                dst_row[dst_idx + 1] = clamp_to_byte(g);
                dst_row[dst_idx + 2] = clamp_to_byte(r);
            }
        } else {
            for (int x = 0; x < dst_w; ++x) {
                const CubicSample1D& sx = x_samples[x];
                float val = 0.0f;
                for (int n = 0; n < 4; ++n) {
                    float row_val = 0.0f;
                    const uint8_t* r_ptr = rows[n];
                    for (int m = 0; m < 4; ++m) {
                        row_val += sx.w[m] * r_ptr[sx.idx[m]];
                    }
                    val += wy[n] * row_val;
                }
                dst_row[x] = clamp_to_byte(val);
            }
        }
    };

    if (policy == ExecutionPolicy::OPENMP_DYNAMIC) {
        #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    } else if (policy == ExecutionPolicy::OPENMP_GUIDED) {
        #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    } else { // OPENMP_STATIC or default
        #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
        for (int y = 0; y < dst_h; ++y) process_row(y);
    }
}

// ============================================================================
// TOP LEVEL UPSCALE DISPATCHER & MEASUREMENT
// ============================================================================

ExecutionMetrics FrameInterpolator::upscale(const cv::Mat& src, cv::Mat& dst, double scale_factor, const InterpolationConfig& config) {
    if (src.empty()) {
        throw std::invalid_argument("Source frame is empty.");
    }
    if (scale_factor <= 0.0) {
        throw std::invalid_argument("Scale factor must be greater than zero.");
    }

    double t_start = omp_get_wtime();

    if (config.policy == ExecutionPolicy::SERIAL) {
        switch (config.method) {
            case InterpolationMethod::NEAREST_NEIGHBOR:
                upscale_nearest_serial(src, dst, scale_factor);
                break;
            case InterpolationMethod::BILINEAR:
                upscale_bilinear_serial(src, dst, scale_factor);
                break;
            case InterpolationMethod::BICUBIC:
                upscale_bicubic_serial(src, dst, scale_factor, config.bicubic_a);
                break;
        }
    } else {
        switch (config.method) {
            case InterpolationMethod::NEAREST_NEIGHBOR:
                upscale_nearest_omp(src, dst, scale_factor, config.policy, config.num_threads, config.chunk_size);
                break;
            case InterpolationMethod::BILINEAR:
                upscale_bilinear_omp(src, dst, scale_factor, config.policy, config.num_threads, config.chunk_size);
                break;
            case InterpolationMethod::BICUBIC:
                upscale_bicubic_omp(src, dst, scale_factor, config.policy, config.num_threads, config.chunk_size, config.bicubic_a);
                break;
        }
    }

    double t_end = omp_get_wtime();
    double elapsed_ms = (t_end - t_start) * 1000.0;

    ExecutionMetrics metrics;
    metrics.execution_time_ms = elapsed_ms;
    metrics.fps = elapsed_ms > 0.0001 ? (1000.0 / elapsed_ms) : 0.0;
    metrics.src_width = src.cols;
    metrics.src_height = src.rows;
    metrics.dst_width = dst.cols;
    metrics.dst_height = dst.rows;
    metrics.scale_factor = scale_factor;
    metrics.method = config.method;
    metrics.policy = config.policy;
    metrics.threads_used = (config.policy == ExecutionPolicy::SERIAL) ? 1 : config.num_threads;

    return metrics;
}

