import os
import sys
import ctypes
import time
import numpy as np
import cv2

# Ensure OpenMP runtime DLL directory is added for Windows
bin_dir = r'C:\Users\Mandar\.gemini\antigravity\scratch\tools\mingw64\bin'
if os.path.exists(bin_dir):
    try:
        os.add_dll_directory(bin_dir)
    except Exception as e:
        pass

dll_path = r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\backend\bin\adaptive_core.dll'
if not os.path.exists(dll_path):
    dll_path = r'C:\Users\Mandar\.gemini\antigravity\scratch\adaptive_core.dll'

class NativeUpscaler:
    def __init__(self):
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f'Native DLL not found at: {dll_path}')
        
        self.lib = ctypes.CDLL(dll_path)
        self.lib.upscale_frame_cpp.argtypes = [
            ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
            ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
            ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int
        ]
        self.lib.upscale_frame_cpp.restype = ctypes.c_double

        self.method_map = {'NEAREST': 0, 'BILINEAR': 1, 'BICUBIC': 2}
        self.policy_map = {'SERIAL': 0, 'OPENMP_STATIC': 1, 'OPENMP_DYNAMIC': 2, 'OPENMP_GUIDED': 3}

    def upscale(self, src_img, scale_factor=2.0, method='BILINEAR', policy='OPENMP_STATIC', num_threads=4, chunk_size=16, workload_multiplier=1):
        if src_img is None or src_img.size == 0:
            raise ValueError('Input frame is empty')
        
        src_h, src_w = src_img.shape[:2]
        channels = 3 if len(src_img.shape) == 3 else 1
        
        dst_w = int(round(src_w * scale_factor))
        dst_h = int(round(src_h * scale_factor))
        
        # Ensure contiguous memory buffer
        if not src_img.flags['C_CONTIGUOUS']:
            src_img = np.ascontiguousarray(src_img)
            
        dst_shape = (dst_h, dst_w, channels) if channels == 3 else (dst_h, dst_w)
        dst_img = np.zeros(dst_shape, dtype=np.uint8)

        m_code = self.method_map.get(method.upper(), 1)
        p_code = self.policy_map.get(policy.upper(), 1)

        total_latency = 0.0
        # Controlled artificial workload multiplier (Section 10)
        multiplier = max(1, int(workload_multiplier))
        for _ in range(multiplier):
            lat = self.lib.upscale_frame_cpp(
                src_img.ctypes.data, src_w, src_h, channels,
                dst_img.ctypes.data, dst_w, dst_h,
                m_code, p_code, int(num_threads), int(chunk_size)
            )
            total_latency += lat

        return dst_img, total_latency
