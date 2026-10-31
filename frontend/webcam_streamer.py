import threading
import time
import math
import cv2
import numpy as np
from upscaler_engine import NativeUpscaler
from adaptive_engine import AdaptiveEngine

class WebcamStreamer:
    def __init__(self):
        self.upscaler = NativeUpscaler()
        self.adaptive_engine = AdaptiveEngine(target_fps=30.0, window_size=10, cooldown_frames=12)

        self.cap = None
        self.is_running = False
        self.use_synthetic = False

        # Config
        self.scale_factor = 2.0
        self.manual_method = 'BICUBIC'
        self.policy = 'OPENMP_STATIC'
        self.num_threads = 4
        self.chunk_size = 16
        self.target_fps = 30.0
        self.workload_multiplier = 1
        self.show_heatmap = False

        # Synchronisation
        self.capture_lock = threading.Lock()
        self.output_lock = threading.Lock()
        self.raw_frame = None
        self.raw_capture_time = 0.0
        self.new_raw_available = False

        self.latest_raw_jpeg = None
        self.latest_upscaled_jpeg = None

        # Metrics & Quality Analysis
        self.frame_index = 0
        self.captured_count = 0
        self.processed_count = 0
        self.discarded_count = 0
        self.last_interp_latency = 0.0
        self.last_end_to_end_latency = 0.0

        self.raw_sharpness = 120.0
        self.upscaled_sharpness = 380.0
        self.raw_res_str = '640x360'
        self.upscaled_res_str = '1280x720'

        self.capture_thread = None
        self.process_thread = None

    def start(self, camera_id=0, width=640, height=480):
        if self.is_running:
            return True

        self.cap = None
        try:
            self.cap = cv2.VideoCapture(camera_id, cv2.CAP_DSHOW)
            if not self.cap.isOpened():
                self.cap.release()
                self.cap = cv2.VideoCapture(camera_id)
        except Exception:
            self.cap = cv2.VideoCapture(camera_id)

        if self.cap and self.cap.isOpened():
            if width > 0 and height > 0:
                self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
                self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
            ret, test_frame = self.cap.read()
            if not ret or test_frame is None:
                print(f'[WARN] Hardware camera index {camera_id} returned empty frame. Activating dynamic synthetic test stream.')
                self.cap.release()
                self.cap = None
                self.use_synthetic = True
            else:
                self.use_synthetic = False
                print(f'[INFO] Hardware camera index {camera_id} connected successfully ({test_frame.shape[1]}x{test_frame.shape[0]}).')
        else:
            print(f'[WARN] Camera index {camera_id} not available. Activating dynamic synthetic test stream.')
            self.use_synthetic = True

        # Pre-generate initial frames
        init_frame = self._generate_synthetic_frame()
        init_up, _ = self.upscaler.upscale(init_frame, scale_factor=self.scale_factor, method=self.manual_method)
        _, init_raw_jpg = cv2.imencode('.jpg', init_frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        _, init_up_jpg = cv2.imencode('.jpg', init_up, [cv2.IMWRITE_JPEG_QUALITY, 80])
        with self.output_lock:
            self.latest_raw_jpeg = init_raw_jpg.tobytes()
            self.latest_upscaled_jpeg = init_up_jpg.tobytes()

        self.is_running = True
        self.frame_index = 0
        self.captured_count = 0
        self.processed_count = 0
        self.discarded_count = 0

        self.capture_thread = threading.Thread(target=self._capture_worker, daemon=True)
        self.process_thread = threading.Thread(target=self._process_worker, daemon=True)

        self.capture_thread.start()
        self.process_thread.start()
        return True

    def stop(self):
        self.is_running = False
        if self.capture_thread and self.capture_thread.is_alive():
            self.capture_thread.join(timeout=1.0)
        if self.process_thread and self.process_thread.is_alive():
            self.process_thread.join(timeout=1.0)
        if self.cap and self.cap.isOpened():
            self.cap.release()
            self.cap = None

    def set_controls(self, scale=None, method=None, policy=None, threads=None, target_fps=None, adaptive=None, workload=None, show_heatmap=None):
        if scale is not None:
            self.scale_factor = float(scale)
        if method is not None:
            self.manual_method = method.upper()
            self.adaptive_engine.set_manual_method(self.manual_method)
        if policy is not None:
            self.policy = policy.upper()
        if threads is not None:
            self.num_threads = int(threads)
        if target_fps is not None:
            self.target_fps = float(target_fps)
            self.adaptive_engine.set_target_fps(self.target_fps)
        if adaptive is not None:
            self.adaptive_engine.set_adaptive_enabled(bool(adaptive))
        if workload is not None:
            w_str = str(workload).upper()
            self.adaptive_engine.set_workload_mode(w_str)
            if w_str == 'NORMAL': self.workload_multiplier = 1
            elif w_str == 'INCREASED': self.workload_multiplier = 2
            elif w_str == 'HIGH': self.workload_multiplier = 4
        if show_heatmap is not None:
            self.show_heatmap = bool(show_heatmap)

    def _generate_synthetic_frame(self, w=640, h=360):
        t = time.time()
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[:] = (20, 22, 28)

        # Subtle background grid
        for x in range(0, w, 40): cv2.line(img, (x, 0), (x, h), (32, 35, 42), 1)
        for y in range(0, h, 40): cv2.line(img, (0, y), (w, y), (32, 35, 42), 1)

        cx, cy = 320, 180
        # High-frequency radial spoke wheel in center (demonstrates antialiasing & subpixel smoothness)
        for deg in range(0, 360, 15):
            rad = math.radians(deg)
            x2 = int(cx + 85 * math.cos(rad))
            y2 = int(cy + 85 * math.sin(rad))
            cv2.line(img, (cx, cy), (x2, y2), (0, 185, 255), 1, cv2.LINE_AA)

        # Concentric rings
        for r in range(15, 90, 15):
            cv2.circle(img, (cx, cy), r, (220, 220, 220), 1, cv2.LINE_AA)

        # Dynamic moving target crosshairs with sub-pixel trajectory
        mx = int(cx + np.sin(t * 2.5) * 170)
        my = int(cy + np.cos(t * 2.5) * 65)
        cv2.circle(img, (mx, my), 28, (0, 0, 255), 2)
        cv2.circle(img, (mx, my), 14, (255, 255, 255), 1)
        cv2.line(img, (mx - 35, my), (mx + 35, my), (0, 255, 0), 1)
        cv2.line(img, (mx, my - 35), (mx, my + 35), (0, 255, 0), 1)

        # High-contrast 8px checkerboard (tests high-frequency contrast & Nyquist boundary)
        for y in range(250, 330, 8):
            for x in range(30, 150, 8):
                if (x // 8 + y // 8) % 2 == 0:
                    cv2.rectangle(img, (x, y), (x + 8, y + 8), (240, 240, 240), -1)
        cv2.putText(img, '8px Checkerboard', (30, 240), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (160, 160, 160), 1)

        # Multi-scale fine text (sharpness and legibility test)
        cur_time_str = time.strftime('%H:%M:%S')
        cv2.putText(img, 'ADAPTIVE VIDEO UPSCALING', (20, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.72, (255, 255, 255), 2)
        cv2.putText(img, f'Resolution Test Stream | Time: {cur_time_str} | Frame: {self.captured_count}', (20, 58), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 255, 200), 1)
        cv2.putText(img, 'Curvature: Catmull-Rom Bicubic vs Bilinear Interpolation', (20, 78), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200, 200, 200), 1)
        cv2.putText(img, 'Fine 10pt Text: ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789', (20, 96), cv2.FONT_HERSHEY_SIMPLEX, 0.33, (255, 255, 0), 1)
        cv2.putText(img, 'Microscopic 6pt Test (Use Zoom Lens Below To Inspect Sub-Pixels)', (20, 112), cv2.FONT_HERSHEY_SIMPLEX, 0.28, (150, 255, 150), 1)

        # Outer border
        cv2.rectangle(img, (10, 10), (w - 10, h - 10), (100, 100, 120), 1)
        return img

    def _capture_worker(self):
        target_interval = 1.0 / self.target_fps
        while self.is_running:
            t0 = time.time()
            if self.use_synthetic:
                frame = self._generate_synthetic_frame()
                ret = True
            else:
                ret, frame = self.cap.read()

            if not ret or frame is None:
                time.sleep(0.01)
                continue

            cap_time = time.time()
            self.captured_count += 1

            with self.capture_lock:
                if self.new_raw_available:
                    self.discarded_count += 1
                self.raw_frame = frame
                self.raw_capture_time = cap_time
                self.new_raw_available = True

            elapsed = time.time() - t0
            sleep_time = target_interval - elapsed
            if sleep_time > 0.001:
                time.sleep(sleep_time)

    def _process_worker(self):
        while self.is_running:
            frame_to_upscale = None
            cap_time = 0.0

            with self.capture_lock:
                if self.new_raw_available:
                    frame_to_upscale = self.raw_frame
                    cap_time = self.raw_capture_time
                    self.new_raw_available = False

            if frame_to_upscale is None:
                time.sleep(0.002)
                continue

            self.frame_index += 1
            self.processed_count += 1

            active_method = self.manual_method
            if self.adaptive_engine.adaptive_enabled:
                active_method = self.adaptive_engine.current_method

            t_start = time.time()
            upscaled_frame, interp_latency = self.upscaler.upscale(
                frame_to_upscale,
                scale_factor=self.scale_factor,
                method=active_method,
                policy=self.policy,
                num_threads=self.num_threads,
                chunk_size=self.chunk_size,
                workload_multiplier=self.workload_multiplier
            )
            t_end = time.time()

            end_to_end_lat = (t_end - cap_time) * 1000.0
            self.last_interp_latency = interp_latency
            self.last_end_to_end_latency = end_to_end_lat

            if self.adaptive_engine.adaptive_enabled:
                self.adaptive_engine.update_and_evaluate(self.frame_index, interp_latency)

            # Measure edge sharpness variance every 6 frames
            if self.frame_index % 6 == 0:
                try:
                    gray_up = cv2.cvtColor(upscaled_frame, cv2.COLOR_BGR2GRAY)
                    gx_u = cv2.Sobel(gray_up, cv2.CV_64F, 1, 0)
                    gy_u = cv2.Sobel(gray_up, cv2.CV_64F, 0, 1)
                    total_up_energy = float(np.sum(gx_u**2 + gy_u**2))
                    self.upscaled_sharpness = round(total_up_energy / 1e9, 2) # In Billions

                    gray_raw = cv2.cvtColor(frame_to_upscale, cv2.COLOR_BGR2GRAY)
                    gx_r = cv2.Sobel(gray_raw, cv2.CV_64F, 1, 0)
                    gy_r = cv2.Sobel(gray_raw, cv2.CV_64F, 0, 1)
                    total_raw_energy = float(np.sum(gx_r**2 + gy_r**2))
                    self.raw_sharpness = round(total_raw_energy / 1e9, 2) # In Billions

                    self.raw_res_str = f'{frame_to_upscale.shape[1]}x{frame_to_upscale.shape[0]}'
                    self.upscaled_res_str = f'{upscaled_frame.shape[1]}x{upscaled_frame.shape[0]}'
                except Exception:
                    pass

            # If heatmap mode is active, overlay reconstructed high-frequency gradients
            display_upscaled = upscaled_frame
            if self.show_heatmap:
                try:
                    gray = cv2.cvtColor(upscaled_frame, cv2.COLOR_BGR2GRAY)
                    lap = cv2.Laplacian(gray, cv2.CV_8U, ksize=3)
                    heat = cv2.applyColorMap(lap * 3, cv2.COLORMAP_JET)
                    display_upscaled = cv2.addWeighted(upscaled_frame, 0.4, heat, 0.6, 0)
                except Exception:
                    pass

            _, raw_jpeg = cv2.imencode('.jpg', frame_to_upscale, [cv2.IMWRITE_JPEG_QUALITY, 85])
            _, upscaled_jpeg = cv2.imencode('.jpg', display_upscaled, [cv2.IMWRITE_JPEG_QUALITY, 85])

            with self.output_lock:
                self.latest_raw_jpeg = raw_jpeg.tobytes()
                self.latest_upscaled_jpeg = upscaled_jpeg.tobytes()

    def get_raw_jpeg(self):
        with self.output_lock:
            return self.latest_raw_jpeg

    def get_upscaled_jpeg(self):
        with self.output_lock:
            return self.latest_upscaled_jpeg

    def get_metrics(self):
        status = self.adaptive_engine.get_status()
        status['discarded_frames'] = self.discarded_count
        status['processed_frames'] = self.processed_count
        status['captured_frames'] = self.captured_count
        status['interpolation_latency_ms'] = round(self.last_interp_latency, 2)
        status['end_to_end_latency_ms'] = round(self.last_end_to_end_latency, 2)
        status['scale_factor'] = self.scale_factor
        status['policy'] = self.policy
        status['num_threads'] = 1 if self.policy == 'SERIAL' else self.num_threads
        status['decisions'] = self.adaptive_engine.decision_history[-10:]

        # Quality Analytics
        status['raw_sharpness'] = self.raw_sharpness
        status['upscaled_sharpness'] = self.upscaled_sharpness
        sharpness_gain = round(((self.upscaled_sharpness - self.raw_sharpness) / max(1.0, self.raw_sharpness)) * 100.0, 1)
        status['sharpness_gain_pct'] = f'+{sharpness_gain}%' if sharpness_gain >= 0 else f'{sharpness_gain}%'

        raw_px = 640 * 360
        up_px = int(round(640 * self.scale_factor)) * int(round(360 * self.scale_factor))
        status['raw_pixels'] = raw_px
        status['upscaled_pixels'] = up_px
        status['pixel_gain_pct'] = f'+{int(round(((up_px - raw_px) / raw_px) * 100))}%'
        status['raw_resolution'] = self.raw_res_str
        status['upscaled_resolution'] = self.upscaled_res_str
        status['show_heatmap'] = self.show_heatmap

        return status
