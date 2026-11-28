import os
import sys
import ctypes
import time
import numpy as np
import cv2

# Ensure OpenMP runtime DLL directory is added for Windows
bin_dir = r"C:\Users\Mandar\.gemini\antigravity\scratch\tools\mingw64\bin"
if os.path.exists(bin_dir):
    try:
        os.add_dll_directory(bin_dir)
    except Exception:
        pass

dll_candidates = [
    r"C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\backend\bin\adaptive_core_v3.dll",
    r"C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\backend\bin\adaptive_core_v2.dll",
    r"C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\backend\bin\adaptive_core.dll",
    r"C:\Users\Mandar\.gemini\antigravity\scratch\adaptive_core.dll"
]
dll_path = next((p for p in dll_candidates if os.path.exists(p)), dll_candidates[0])

def resolve_target_dimensions(src_w, src_h, scale_setting):
    """
    Intelligently resolves target (width, height) based on target resolution:
    - 1080p: Full HD. 16:9 -> 1920x1080, otherwise preserves aspect ratio at h=1080.
    - 720p: HD. 16:9 -> 1280x720, otherwise preserves aspect ratio at h=720.
    - 4k: Ultra HD. 16:9 -> 3840x2160, otherwise preserves aspect ratio at h=2160.
    - 4.0: 4x linear scale.
    - 2.0: 2x linear scale.
    Guarantees even width and height for video codec compliance.
    """
    if src_w <= 0 or src_h <= 0:
        return 1920, 1080

    scale_str = str(scale_setting).lower().strip()
    aspect = float(src_w) / float(src_h)

    if "4k" in scale_str or "2160" in scale_str or "uhd" in scale_str:
        target_h = 2160
        target_w = 3840 if 1.6 <= aspect <= 1.9 else int(round(src_w * (target_h / float(src_h))))
    elif "1080" in scale_str or "fhd" in scale_str:
        target_h = 1080
        target_w = 1920 if 1.6 <= aspect <= 1.9 else int(round(src_w * (target_h / float(src_h))))
    elif "720" in scale_str or "hd" in scale_str:
        target_h = 720
        target_w = 1280 if 1.6 <= aspect <= 1.9 else int(round(src_w * (target_h / float(src_h))))
    else:
        try:
            factor = float(scale_str.replace("x", ""))
            target_w = int(round(src_w * factor))
            target_h = int(round(src_h * factor))
        except Exception:
            target_h = 1080
            target_w = 1920 if 1.6 <= aspect <= 1.9 else int(round(src_w * (target_h / float(src_h))))

    if target_w % 2 != 0:
        target_w += 1
    if target_h % 2 != 0:
        target_h += 1

    target_w = max(target_w, src_w)
    target_h = max(target_h, src_h)
    return target_w, target_h

class NativeUpscaler:
    def __init__(self):
        self.lib = None
        for cand in dll_candidates:
            if os.path.exists(cand):
                try:
                    self.lib = ctypes.CDLL(cand)
                    print(f"Loaded Native C++ Engine: {cand}")
                    break
                except Exception:
                    continue

        if self.lib is not None:
            try:
                self.lib.upscale_frame_cpp.argtypes = [
                    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                    ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                    ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int
                ]
                self.lib.upscale_frame_cpp.restype = ctypes.c_double

                if hasattr(self.lib, 'upscale_and_enhance_cpp'):
                    self.lib.upscale_and_enhance_cpp.argtypes = [
                        ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                        ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_float
                    ]
                    self.lib.upscale_and_enhance_cpp.restype = ctypes.c_double
                else:
                    self.lib.upscale_and_enhance_cpp = None
            except Exception as e:
                print(f"Error binding native DLL functions: {e}")
                self.lib = None

        if self.lib is None:
            print("Native C++ DLL inactive. Utilizing multi-threaded fallback engine.")

        self.method_map = {'NEAREST': 0, 'BILINEAR': 1, 'BICUBIC': 2}
        self.policy_map = {'SERIAL': 0, 'OPENMP_STATIC': 1, 'OPENMP_DYNAMIC': 2, 'OPENMP_GUIDED': 3}

    def upscale(self, src_img, scale_factor="1080p", method="BICUBIC", policy="OPENMP_STATIC",
                num_threads=8, chunk_size=16, workload_multiplier=1, enhancement=3.5):
        if src_img is None or src_img.size == 0:
            raise ValueError('Input frame is empty')

        src_h, src_w = src_img.shape[:2]
        channels = 3 if len(src_img.shape) == 3 else 1

        dst_w, dst_h = resolve_target_dimensions(src_w, src_h, scale_factor)

        # Stage 1: Fast compression artifact suppression for low-resolution inputs (144p, 288p, 360p)
        # Prevents JPEG/H264 blocky macroblocks from being amplified into harsh noise
        if src_h <= 480 and channels == 3:
            pre_input = cv2.bilateralFilter(src_img, d=3, sigmaColor=20, sigmaSpace=20)
        else:
            pre_input = src_img

        t0 = time.perf_counter()
        try:
            strength = float(enhancement)
        except Exception:
            strength = 3.5

        if self.lib is not None:
            if not pre_input.flags['C_CONTIGUOUS']:
                pre_input = np.ascontiguousarray(pre_input)

            dst_shape = (dst_h, dst_w, channels) if channels == 3 else (dst_h, dst_w)
            dst_img = np.zeros(dst_shape, dtype=np.uint8)

            m_code = self.method_map.get(str(method).upper(), 2)
            p_code = self.policy_map.get(str(policy).upper(), 1)

            multiplier = max(1, int(workload_multiplier))
            total_latency = 0.0

            for _ in range(multiplier):
                if strength > 0.0 and self.lib.upscale_and_enhance_cpp is not None:
                    lat = self.lib.upscale_and_enhance_cpp(
                        pre_input.ctypes.data, src_w, src_h, channels,
                        dst_img.ctypes.data, dst_w, dst_h,
                        m_code, p_code, int(num_threads), int(chunk_size),
                        ctypes.c_float(strength)
                    )
                else:
                    lat = self.lib.upscale_frame_cpp(
                        pre_input.ctypes.data, src_w, src_h, channels,
                        dst_img.ctypes.data, dst_w, dst_h,
                        m_code, p_code, int(num_threads), int(chunk_size)
                    )
                total_latency += lat
        else:
            cv2.setNumThreads(int(num_threads))
            m_upper = str(method).upper()
            if m_upper == 'NEAREST':
                interp = cv2.INTER_NEAREST
            elif m_upper == 'BILINEAR':
                interp = cv2.INTER_LINEAR
            else:
                interp = cv2.INTER_LANCZOS4 if dst_h > src_h * 2 else cv2.INTER_CUBIC

            dst_img = cv2.resize(pre_input, (dst_w, dst_h), interpolation=interp)
            total_latency = (time.perf_counter() - t0) * 1000.0

        # Stage 2: Online Enhancer Grade Luminance Micro-Contrast & CAS Sharpening
        # Applies micro-detail reconstruction in the Luminance channel (preserves colors, crystal clear textures)
        if strength > 0.0 and channels == 3:
            try:
                ycrcb = cv2.cvtColor(dst_img, cv2.COLOR_BGR2YCrCb)
                y, cr, cb = cv2.split(ycrcb)
                clip = min(2.5, 1.2 + 0.25 * (strength / 3.5))
                clahe = cv2.createCLAHE(clipLimit=clip, tileGridSize=(8, 8))
                y_clahe = clahe.apply(y)

                # Contrast-adaptive fine edge sharpening
                blur_y = cv2.GaussianBlur(y_clahe, (0, 0), sigmaX=1.2)
                sharp_gain = min(0.55, 0.28 * (strength / 3.5))
                y_sharp = cv2.addWeighted(y_clahe, 1.0 + sharp_gain, blur_y, -sharp_gain, 0)

                dst_img = cv2.cvtColor(cv2.merge([y_sharp, cr, cb]), cv2.COLOR_YCrCb2BGR)
            except Exception:
                pass

        final_lat = max(total_latency, (time.perf_counter() - t0) * 1000.0)
        return dst_img, final_lat
