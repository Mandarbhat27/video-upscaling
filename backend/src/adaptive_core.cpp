#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>
#include <omp.h>
#include <cstdint>

// Bicubic Keys kernel (Catmull-Rom)
static inline float cubic_weight(float x, float a = -0.5f) {
    float ax = std::abs(x);
    if (ax <= 1.0f) {
        return ((a + 2.0f) * ax - (a + 3.0f)) * ax * ax + 1.0f;
    } else if (ax < 2.0f) {
        return ((a * ax - 5.0f * a) * ax + 8.0f * a) * ax - 4.0f * a;
    }
    return 0.0f;
}

static inline uint8_t clamp_to_byte(float val) {
    int ival = static_cast<int>(val + 0.5f);
    return static_cast<uint8_t>(ival < 0 ? 0 : (ival > 255 ? 255 : ival));
}

extern "C" {

__declspec(dllexport) double upscale_frame_cpp(
    const uint8_t* src, int src_w, int src_h, int channels,
    uint8_t* dst, int dst_w, int dst_h,
    int method, int policy, int num_threads, int chunk_size
) {
    double t_start = omp_get_wtime();
    float scale_x = static_cast<float>(dst_w) / static_cast<float>(src_w);
    float scale_y = static_cast<float>(dst_h) / static_cast<float>(src_h);

    if (num_threads <= 0) num_threads = omp_get_max_threads();
    if (chunk_size <= 0) chunk_size = 16;

    // 0 = Nearest Neighbour
    if (method == 0) {
        std::vector<int> map_x(dst_w);
        for (int x = 0; x < dst_w; ++x) {
            float src_xf = (static_cast<float>(x) + 0.5f) / scale_x - 0.5f;
            int sx = static_cast<int>(std::round(src_xf));
            map_x[x] = std::clamp(sx, 0, src_w - 1);
        }

        if (policy == 0) { // Serial
            for (int y = 0; y < dst_h; ++y) {
                float src_yf = (static_cast<float>(y) + 0.5f) / scale_y - 0.5f;
                int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);
                const uint8_t* s_row = src + sy * src_w * channels;
                uint8_t* d_row = dst + y * dst_w * channels;
                for (int x = 0; x < dst_w; ++x) {
                    int p_src = map_x[x] * channels;
                    int p_dst = x * channels;
                    for (int c = 0; c < channels; ++c) d_row[p_dst + c] = s_row[p_src + c];
                }
            }
        } else { // OpenMP
            #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
            for (int y = 0; y < dst_h; ++y) {
                float src_yf = (static_cast<float>(y) + 0.5f) / scale_y - 0.5f;
                int sy = std::clamp(static_cast<int>(std::round(src_yf)), 0, src_h - 1);
                const uint8_t* s_row = src + sy * src_w * channels;
                uint8_t* d_row = dst + y * dst_w * channels;
                for (int x = 0; x < dst_w; ++x) {
                    int p_src = map_x[x] * channels;
                    int p_dst = x * channels;
                    for (int c = 0; c < channels; ++c) d_row[p_dst + c] = s_row[p_src + c];
                }
            }
        }
    }
    // 1 = Bilinear
    else if (method == 1) {
        struct Sample1D { int x0, x1; float w0, w1; };
        std::vector<Sample1D> x_samples(dst_w);
        for (int x = 0; x < dst_w; ++x) {
            float src_xf = (static_cast<float>(x) + 0.5f) / scale_x - 0.5f;
            int x0 = static_cast<int>(std::floor(src_xf));
            float u = src_xf - static_cast<float>(x0);
            x_samples[x].x0 = std::clamp(x0, 0, src_w - 1);
            x_samples[x].x1 = std::clamp(x0 + 1, 0, src_w - 1);
            x_samples[x].w1 = u;
            x_samples[x].w0 = 1.0f - u;
        }

        auto process_bilinear_row = [&](int y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / scale_y - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            float v = src_yf - static_cast<float>(y0);
            float vy0 = 1.0f - v;
            float vy1 = v;

            int sy0 = std::clamp(y0, 0, src_h - 1);
            int sy1 = std::clamp(y0 + 1, 0, src_h - 1);

            const uint8_t* row0 = src + sy0 * src_w * channels;
            const uint8_t* row1 = src + sy1 * src_w * channels;
            uint8_t* dst_row = dst + y * dst_w * channels;

            for (int x = 0; x < dst_w; ++x) {
                const auto& sx = x_samples[x];
                int p00 = sx.x0 * channels;
                int p10 = sx.x1 * channels;

                float w00 = vy0 * sx.w0;
                float w10 = vy0 * sx.w1;
                float w01 = vy1 * sx.w0;
                float w11 = vy1 * sx.w1;

                int dst_idx = x * channels;
                for (int c = 0; c < channels; ++c) {
                    float val = w00 * row0[p00 + c] + w10 * row0[p10 + c] +
                                w01 * row1[p00 + c] + w11 * row1[p10 + c];
                    dst_row[dst_idx + c] = clamp_to_byte(val);
                }
            }
        };

        if (policy == 0) {
            for (int y = 0; y < dst_h; ++y) process_bilinear_row(y);
        } else if (policy == 2) {
            #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bilinear_row(y);
        } else if (policy == 3) {
            #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bilinear_row(y);
        } else {
            #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bilinear_row(y);
        }
    }
    // 2 = Bicubic
    else if (method == 2) {
        struct Cubic1D { int x[4]; float w[4]; };
        std::vector<Cubic1D> x_samples(dst_w);
        for (int x = 0; x < dst_w; ++x) {
            float src_xf = (static_cast<float>(x) + 0.5f) / scale_x - 0.5f;
            int x0 = static_cast<int>(std::floor(src_xf));
            float u = src_xf - static_cast<float>(x0);
            float sum = 0.0f;
            for (int m = 0; m < 4; ++m) {
                x_samples[x].x[m] = std::clamp(x0 + m - 1, 0, src_w - 1);
                x_samples[x].w[m] = cubic_weight(static_cast<float>(m - 1) - u);
                sum += x_samples[x].w[m];
            }
            if (std::abs(sum) > 1e-6f) {
                float inv = 1.0f / sum;
                for (int m = 0; m < 4; ++m) x_samples[x].w[m] *= inv;
            }
        }

        auto process_bicubic_row = [&](int y) {
            float src_yf = (static_cast<float>(y) + 0.5f) / scale_y - 0.5f;
            int y0 = static_cast<int>(std::floor(src_yf));
            float v = src_yf - static_cast<float>(y0);

            int y_idx[4];
            float wy[4];
            float sum_y = 0.0f;
            for (int n = 0; n < 4; ++n) {
                y_idx[n] = std::clamp(y0 + n - 1, 0, src_h - 1);
                wy[n] = cubic_weight(static_cast<float>(n - 1) - v);
                sum_y += wy[n];
            }
            if (std::abs(sum_y) > 1e-6f) {
                float inv_y = 1.0f / sum_y;
                for (int n = 0; n < 4; ++n) wy[n] *= inv_y;
            }

            const uint8_t* rows[4] = {
                src + y_idx[0] * src_w * channels,
                src + y_idx[1] * src_w * channels,
                src + y_idx[2] * src_w * channels,
                src + y_idx[3] * src_w * channels
            };
            uint8_t* dst_row = dst + y * dst_w * channels;

            for (int x = 0; x < dst_w; ++x) {
                const auto& sx = x_samples[x];
                int dst_idx = x * channels;
                for (int c = 0; c < channels; ++c) {
                    float val = 0.0f;
                    for (int n = 0; n < 4; ++n) {
                        float row_val = 0.0f;
                        const uint8_t* r_ptr = rows[n];
                        for (int m = 0; m < 4; ++m) {
                            row_val += sx.w[m] * r_ptr[sx.x[m] * channels + c];
                        }
                        val += wy[n] * row_val;
                    }
                    dst_row[dst_idx + c] = clamp_to_byte(val);
                }
            }
        };

        if (policy == 0) {
            for (int y = 0; y < dst_h; ++y) process_bicubic_row(y);
        } else if (policy == 2) {
            #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bicubic_row(y);
        } else if (policy == 3) {
            #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bicubic_row(y);
        } else {
            #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
            for (int y = 0; y < dst_h; ++y) process_bicubic_row(y);
        }
    }

    double t_end = omp_get_wtime();
    return (t_end - t_start) * 1000.0;
}

// High-Frequency Contrast & Edge Enhancement Filter using OpenMP
__declspec(dllexport) double enhance_edges_cpp(
    const uint8_t* src, uint8_t* dst, int width, int height, int channels,
    float strength, int policy, int num_threads, int chunk_size
) {
    double t_start = omp_get_wtime();
    if (num_threads <= 0) num_threads = omp_get_max_threads();
    if (chunk_size <= 0) chunk_size = 16;
    if (strength <= 0.0f) {
        if (src != dst) {
            std::memcpy(dst, src, static_cast<size_t>(width) * height * channels);
        }
        return (omp_get_wtime() - t_start) * 1000.0;
    }

    // 8-neighbourhood normalized high-pass sharpening + micro-contrast boost
    float w = strength * 0.28f;
    float w_diag = -0.5f * w;
    float w_ortho = -w;
    float w_center = 1.0f + 6.0f * w;
    float cboost = (strength > 0.3f) ? (1.0f + 0.055f * std::min(strength, 2.5f)) : 1.0f;

    // Boundary copy (row 0 and row height - 1)
    for (int c = 0; c < channels; ++c) {
        for (int x = 0; x < width; ++x) {
            dst[x * channels + c] = src[x * channels + c];
            int b_idx = ((height - 1) * width + x) * channels + c;
            dst[b_idx] = src[b_idx];
        }
    }

    auto process_sharpen_row = [&](int y) {
        const uint8_t* row_prev = src + (y - 1) * width * channels;
        const uint8_t* row_curr = src + y * width * channels;
        const uint8_t* row_next = src + (y + 1) * width * channels;
        uint8_t* dst_row = dst + y * width * channels;

        // Left and right border pixels
        for (int c = 0; c < channels; ++c) {
            dst_row[c] = row_curr[c];
            int right_idx = (width - 1) * channels + c;
            dst_row[right_idx] = row_curr[right_idx];
        }

        // Interior pixels
        for (int x = 1; x < width - 1; ++x) {
            int idx_p = (x - 1) * channels;
            int idx_c = x * channels;
            int idx_n = (x + 1) * channels;

            for (int c = 0; c < channels; ++c) {
                float val = 
                    w_diag * (row_prev[idx_p + c] + row_prev[idx_n + c] + row_next[idx_p + c] + row_next[idx_n + c]) +
                    w_ortho * (row_prev[idx_c + c] + row_curr[idx_p + c] + row_curr[idx_n + c] + row_next[idx_c + c]) +
                    w_center * row_curr[idx_c + c];
                if (cboost > 1.0f) {
                    val = (val - 128.0f) * cboost + 128.0f;
                }
                dst_row[idx_c + c] = clamp_to_byte(val);
            }
        }
    };

    if (policy == 0) { // Serial
        for (int y = 1; y < height - 1; ++y) process_sharpen_row(y);
    } else if (policy == 2) { // OpenMP Dynamic
        #pragma omp parallel for num_threads(num_threads) schedule(dynamic, chunk_size)
        for (int y = 1; y < height - 1; ++y) process_sharpen_row(y);
    } else if (policy == 3) { // OpenMP Guided
        #pragma omp parallel for num_threads(num_threads) schedule(guided, chunk_size)
        for (int y = 1; y < height - 1; ++y) process_sharpen_row(y);
    } else { // OpenMP Static
        #pragma omp parallel for num_threads(num_threads) schedule(static, chunk_size)
        for (int y = 1; y < height - 1; ++y) process_sharpen_row(y);
    }

    double t_end = omp_get_wtime();
    return (t_end - t_start) * 1000.0;
}

// Unified Super-Resolution: Sub-pixel Interpolation + OpenMP Edge Sharpening (Zero-Allocation Fast Path)
__declspec(dllexport) double upscale_and_enhance_cpp(
    const uint8_t* src, int src_w, int src_h, int channels,
    uint8_t* dst, int dst_w, int dst_h,
    int method, int policy, int num_threads, int chunk_size,
    float enhancement_strength
) {
    if (enhancement_strength <= 0.0f) {
        return upscale_frame_cpp(src, src_w, src_h, channels, dst, dst_w, dst_h, method, policy, num_threads, chunk_size);
    }

    size_t total_bytes = static_cast<size_t>(dst_w) * dst_h * channels;
    static thread_local std::vector<uint8_t> tls_temp_buf;
    if (tls_temp_buf.size() < total_bytes) {
        tls_temp_buf.resize(total_bytes);
    }

    double t1 = upscale_frame_cpp(src, src_w, src_h, channels, tls_temp_buf.data(), dst_w, dst_h, method, policy, num_threads, chunk_size);
    double t2 = enhance_edges_cpp(tls_temp_buf.data(), dst, dst_w, dst_h, channels, enhancement_strength, policy, num_threads, chunk_size);
    return t1 + t2;
}

} // extern "C"
