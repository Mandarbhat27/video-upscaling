import os
import json
import csv
import time
import numpy as np
import cv2
from upscaler_engine import NativeUpscaler

def compute_psnr(ref, target):
    diff = np.abs(ref.astype(np.float64) - target.astype(np.float64))
    mse = np.mean(diff ** 2)
    if mse <= 1e-10:
        return 100.0
    return 10.0 * np.log10((255.0 ** 2) / mse)

def compute_ssim(ref, target):
    C1 = (0.01 * 255) ** 2
    C2 = (0.03 * 255) ** 2
    img1 = ref.astype(np.float64)
    img2 = target.astype(np.float64)
    
    kernel = cv2.getGaussianKernel(11, 1.5)
    window = np.outer(kernel, kernel.transpose())
    
    mu1 = cv2.filter2D(img1, -1, window)[5:-5, 5:-5]
    mu2 = cv2.filter2D(img2, -1, window)[5:-5, 5:-5]
    
    mu1_sq = mu1 ** 2
    mu2_sq = mu2 ** 2
    mu1_mu2 = mu1 * mu2
    
    sigma1_sq = cv2.filter2D(img1 ** 2, -1, window)[5:-5, 5:-5] - mu1_sq
    sigma2_sq = cv2.filter2D(img2 ** 2, -1, window)[5:-5, 5:-5] - mu2_sq
    sigma12 = cv2.filter2D(img1 * img2, -1, window)[5:-5, 5:-5] - mu1_mu2
    
    ssim_map = ((2 * mu1_mu2 + C1) * (2 * sigma12 + C2)) / ((mu1_sq + mu2_sq + C1) * (sigma1_sq + sigma2_sq + C2))
    return float(np.mean(ssim_map))

class BenchmarkRunner:
    def __init__(self):
        self.upscaler = NativeUpscaler()

    def run_full_benchmark(self, input_frame=None, scale_factors=[2.0, 4.0],
                           thread_list=[1, 2, 4, 8, 12], iterations=5):
        if input_frame is None:
            # Generate a 640x360 test frame with rich edges and textures
            h, w = 360, 640
            input_frame = np.zeros((h, w, 3), dtype=np.uint8)
            for y in range(h):
                for x in range(w):
                    input_frame[y, x] = [
                        (x * 255) // w,
                        (y * 255) // h,
                        ((x + y) * 255) // (w + h)
                    ]
            cv2.circle(input_frame, (w // 2, h // 2), 80, (255, 255, 255), -1)
            cv2.putText(input_frame, 'BENCHMARK REFERENCE', (50, 100), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 3)

        src_h, src_w = input_frame.shape[:2]
        methods = ['NEAREST', 'BILINEAR', 'BICUBIC']
        policies = ['OPENMP_STATIC', 'OPENMP_DYNAMIC', 'OPENMP_GUIDED']
        records = []

        print(f'Starting automated benchmark on {src_w}x{src_h} frame...')

        for scale in scale_factors:
            serial_baselines = {}
            # 1. Run Serial Baseline
            for m in methods:
                # Warmup
                self.upscaler.upscale(input_frame, scale_factor=scale, method=m, policy='SERIAL', num_threads=1)
                times = []
                for _ in range(iterations):
                    _, lat = self.upscaler.upscale(input_frame, scale_factor=scale, method=m, policy='SERIAL', num_threads=1)
                    times.append(lat)
                avg_lat = float(np.mean(times))
                serial_baselines[m] = avg_lat
                fps = 1000.0 / avg_lat

                dst_w = int(round(src_w * scale))
                dst_h = int(round(src_h * scale))

                rec = {
                    'method': m,
                    'policy': 'SERIAL',
                    'threads': 1,
                    'scale_factor': scale,
                    'src_resolution': f'{src_w}x{src_h}',
                    'dst_resolution': f'{dst_w}x{dst_h}',
                    'avg_latency_ms': round(avg_lat, 2),
                    'min_latency_ms': round(float(np.min(times)), 2),
                    'max_latency_ms': round(float(np.max(times)), 2),
                    'stddev_latency_ms': round(float(np.std(times)), 2),
                    'fps': round(fps, 1),
                    'speedup': 1.0,
                    'parallel_efficiency': 1.0
                }
                records.append(rec)
                print(f'SERIAL | {m:8s} | Scale {scale}x | Latency: {avg_lat:6.2f}ms | FPS: {fps:5.1f}')

            # 2. Run OpenMP Parallel Policies
            for m in methods:
                t_ser = serial_baselines[m]
                for pol in policies:
                    for th in thread_list:
                        # Warmup
                        self.upscaler.upscale(input_frame, scale_factor=scale, method=m, policy=pol, num_threads=th)
                        times = []
                        for _ in range(iterations):
                            _, lat = self.upscaler.upscale(input_frame, scale_factor=scale, method=m, policy=pol, num_threads=th)
                            times.append(lat)
                        avg_lat = float(np.mean(times))
                        fps = 1000.0 / avg_lat
                        speedup = t_ser / avg_lat
                        eff = speedup / th

                        dst_w = int(round(src_w * scale))
                        dst_h = int(round(src_h * scale))

                        rec = {
                            'method': m,
                            'policy': pol,
                            'threads': th,
                            'scale_factor': scale,
                            'src_resolution': f'{src_w}x{src_h}',
                            'dst_resolution': f'{dst_w}x{dst_h}',
                            'avg_latency_ms': round(avg_lat, 2),
                            'min_latency_ms': round(float(np.min(times)), 2),
                            'max_latency_ms': round(float(np.max(times)), 2),
                            'stddev_latency_ms': round(float(np.std(times)), 2),
                            'fps': round(fps, 1),
                            'speedup': round(speedup, 2),
                            'parallel_efficiency': round(eff, 3)
                        }
                        records.append(rec)

        return records

    def export_results(self, records, output_dir='results'):
        os.makedirs(output_dir, exist_ok=True)
        json_path = os.path.join(output_dir, 'benchmark_results.json')
        csv_path = os.path.join(output_dir, 'benchmark_results.csv')

        with open(json_path, 'w') as f:
            json.dump(records, f, indent=2)

        if records:
            keys = records[0].keys()
            with open(csv_path, 'w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=keys)
                writer.writeheader()
                writer.writerows(records)

        print(f'Exported {len(records)} benchmark records to {json_path} and {csv_path}')
        return json_path, csv_path
