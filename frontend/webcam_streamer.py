import os
import threading
import time
import math
import cv2
import numpy as np
from upscaler_engine import NativeUpscaler
from adaptive_engine import AdaptiveEngine

def precise_sleep(duration):
    if duration <= 0.0 or duration > 1.0:
        return
    target = time.perf_counter() + duration
    if duration > 0.003:
        time.sleep(duration - 0.002)
    while time.perf_counter() < target:
        pass

class WebcamStreamer:
    def __init__(self):
        self.upscaler = NativeUpscaler()
        self.adaptive_engine = AdaptiveEngine(target_fps=120.0, window_size=10, cooldown_frames=12)

        self.cap = None
        self.is_running = False
        self.use_synthetic = False

        # Config
        self.scale_factor = '1080p'
        self.manual_method = 'BICUBIC'
        self.policy = 'OPENMP_STATIC'
        self.num_threads = 8
        self.chunk_size = 16
        self.target_fps = 120.0
        self.workload_multiplier = 1
        self.enhancement_strength = 3.5
        self.num_threads = 8  # OpenMP edge sharpening strength
        self.show_heatmap = False

        # Lighting & Sensor Adjustments (Section 1.4 Objectives)
        self.brightness = 0           # -50 to +50
        self.contrast = 1.0           # 0.5 to 2.0
        self.low_light_boost = False  # CLAHE (Silva et al. [5])
        self.denoise = False          # Denoising filter (Dolwithayaku et al. [4])
        self.clahe = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8))

        # Stream Mode & Source
        self.stream_mode = 'webcam'  # 'webcam', 'cctv', 'cctv_url'
        self.stream_url = ''
        self.stream_source_label = 'Local USB Webcam'
        self.is_looping_video = False

        # Live Enhanced Stream Recording Pipeline
        uploads_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'uploads'))
        os.makedirs(uploads_dir, exist_ok=True)
        self.recorded_video_path = os.path.join(uploads_dir, 'live_enhanced_recording.mp4')
        self.video_writer = None
        self.writer_lock = threading.Lock()
        self.recorded_frames_count = 0
        self.has_recorded_video = False

        # Synchronisation
        self.capture_lock = threading.Lock()
        self.output_lock = threading.Lock()
        self.raw_frame = None
        self.raw_capture_time = 0.0
        self.new_raw_available = False
        self.frame_event = threading.Event()

        self.latest_raw_jpeg = None
        self.latest_upscaled_jpeg = None

        # Metrics & Quality Analysis
        self.frame_index = 0
        self.captured_count = 0
        self.processed_count = 0
        self.discarded_count = 0
        self.last_interp_latency = 0.0
        self.last_end_to_end_latency = 0.0

        self.raw_sharpness = 45.0
        self.upscaled_sharpness = 120.0
        self.raw_res_str = '640x360'
        self.upscaled_res_str = '1280x720'

        self.capture_thread = None
        self.process_thread = None

    def start(self, camera_id=0, stream_url=None, mode='webcam', width=640, height=360):
        if self.is_running:
            self.stop()

        self.stream_mode = mode
        self.stream_url = (stream_url or '').strip()
        self.cap = None
        self.is_looping_video = False

        # Reset live recording session
        with self.writer_lock:
            if self.video_writer:
                try:
                    self.video_writer.release()
                except Exception:
                    pass
                self.video_writer = None
            self.recorded_frames_count = 0
            self.has_recorded_video = False

        # Reset recording state
        with self.writer_lock:
            if self.video_writer:
                try:
                    self.video_writer.release()
                except Exception:
                    pass
                self.video_writer = None
            self.recorded_frames_count = 0
            self.has_recorded_video = False

        if mode in ['cctv', 'cctv_url']:
            raw_url = (self.stream_url or '').strip()
            if not raw_url:
                self.last_error_msg = "Please enter your camera stream URL or IP address (e.g. http://192.168.1.38:8080/video)."
                print(f"[ERROR] {self.last_error_msg}")
                self.is_running = False
                return False

            # Smart URL normalization for phone IP Webcam & RTSP streams
            if not (raw_url.startswith('http://') or raw_url.startswith('https://') or raw_url.startswith('rtsp://')):
                raw_url = 'http://' + raw_url

            candidate_urls = [raw_url]

            # If user entered IP Webcam address without /video (e.g. http://192.168.1.38:8080)
            if ':8080' in raw_url:
                base = raw_url.rstrip('/')
                if not base.endswith('/video') and not base.endswith('/shot.jpg'):
                    candidate_urls.insert(0, base + '/video')
                    candidate_urls.append(base + '/video?dummy=param.mjpg')
                    candidate_urls.append(base + '/shot.jpg')

            connected = False
            for test_u in candidate_urls:
                try:
                    print(f"[INFO] Connecting to real-time live camera: {test_u}")
                    cap = cv2.VideoCapture(test_u)
                    if cap.isOpened():
                        # Set low buffer size to eliminate stream lag
                        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                        ret, test_frame = cap.read()
                        if ret and test_frame is not None and test_frame.size > 0:
                            self.cap = cap
                            self.stream_url = test_u
                            self.use_synthetic = False
                            self.is_looping_video = False
                            self.stream_source_label = f'Live Phone Camera: {test_u}'
                            print(f"[INFO] Successfully connected to REAL-TIME camera feed: {test_u} ({test_frame.shape[1]}x{test_frame.shape[0]})")
                            connected = True
                            break
                        else:
                            cap.release()
                except Exception as e:
                    print(f"[WARN] Failed probing {test_u}: {e}")

            if not connected:
                self.cap = None
                self.is_running = False
                self.use_synthetic = False
                self.is_looping_video = False
                self.last_error_msg = f"Could not connect to {raw_url}. Please ensure: 1) Your phone IP Webcam server is running. 2) Phone and laptop are on the same Wi-Fi. 3) Address is http://{raw_url.split('://')[-1].rstrip('/')}/video"
                print(f"[ERROR] {self.last_error_msg}")
                return False
        else:
            # Standard Webcam Mode
            self.stream_mode = 'webcam'
            self.stream_source_label = 'Local USB Webcam'
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
        init_up, _ = self.upscaler.upscale(
            init_frame, scale_factor=self.scale_factor, method=self.manual_method,
            enhancement=self.enhancement_strength
        )
        _, init_raw_jpg = cv2.imencode('.jpg', init_frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        _, init_up_jpg = cv2.imencode('.jpg', init_up, [cv2.IMWRITE_JPEG_QUALITY, 80])
        with self.output_lock:
            self.latest_raw_jpeg = init_raw_jpg.tobytes()
            self.latest_upscaled_jpeg = init_up_jpg.tobytes()

        self.is_running = True
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

        if self.cap:
            try:
                self.cap.release()
            except Exception:
                pass
            self.cap = None

        # Finalize and close recorded live video
        with self.writer_lock:
            if self.video_writer:
                try:
                    self.video_writer.release()
                except Exception as e:
                    print(f'[WARN] Error releasing video writer: {e}')
                self.video_writer = None

            if os.path.exists(self.recorded_video_path) and os.path.getsize(self.recorded_video_path) > 1024 and self.recorded_frames_count > 0:
                self.has_recorded_video = True
                print(f'[INFO] Saved live enhanced video session ({self.recorded_frames_count} frames, {os.path.getsize(self.recorded_video_path)} bytes) to {self.recorded_video_path}')
            else:
                self.has_recorded_video = False

    def set_controls(self, scale=None, method=None, policy=None, threads=None,
                     target_fps=None, adaptive=None, workload=None, show_heatmap=None,
                     enhancement=None, brightness=None, contrast=None,
                     low_light_boost=None, denoise=None):
        if scale is not None:
            self.scale_factor = str(scale)
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
        if enhancement is not None:
            self.enhancement_strength = float(enhancement)
        if brightness is not None:
            self.brightness = int(brightness)
        if contrast is not None:
            self.contrast = float(contrast)
        if low_light_boost is not None:
            self.low_light_boost = bool(low_light_boost)
        if denoise is not None:
            self.denoise = bool(denoise)

    def _generate_synthetic_frame(self, w=640, h=360):
        t = time.time()
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[:] = (20, 22, 28)

        # Background grid
        for x in range(0, w, 40): cv2.line(img, (x, 0), (x, h), (32, 35, 42), 1)
        for y in range(0, h, 40): cv2.line(img, (0, y), (w, y), (32, 35, 42), 1)

        cx, cy = 320, 180
        # High-frequency radial spoke wheel
        for deg in range(0, 360, 15):
            rad = math.radians(deg)
            x2 = int(cx + 85 * math.cos(rad))
            y2 = int(cy + 85 * math.sin(rad))
            cv2.line(img, (cx, cy), (x2, y2), (0, 185, 255), 1, cv2.LINE_AA)

        for r in range(15, 90, 15):
            cv2.circle(img, (cx, cy), r, (220, 220, 220), 1, cv2.LINE_AA)

        mx = int(cx + np.sin(t * 2.5) * 170)
        my = int(cy + np.cos(t * 2.5) * 65)
        cv2.circle(img, (mx, my), 28, (0, 0, 255), 2)
        cv2.circle(img, (mx, my), 14, (255, 255, 255), 1)
        cv2.line(img, (mx - 35, my), (mx + 35, my), (0, 255, 0), 1)
        cv2.line(img, (mx, my - 35), (mx, my + 35), (0, 255, 0), 1)

        for y in range(250, 330, 8):
            for x in range(30, 150, 8):
                if (x // 8 + y // 8) % 2 == 0:
                    cv2.rectangle(img, (x, y), (x + 8, y + 8), (240, 240, 240), -1)
        cv2.putText(img, '8px Checkerboard', (30, 240), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (160, 160, 160), 1)

        cur_time_str = time.strftime('%H:%M:%S') + f'.{int((t % 1) * 1000):03d}'
        cv2.putText(img, 'ULTRA HIGH-SPEED 120 FPS PARALLEL SUPER-RESOLUTION', (20, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 255, 255), 2)
        cv2.putText(img, f'Resolution: 640x360 -> 1280x720 | 120 FPS Budget: 8.33ms | Time: {cur_time_str}', (20, 58), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 255, 200), 1)
        cv2.putText(img, f'Frame: #{self.captured_count} | C++ AVX2/FMA + OpenMP Parallel Edge Reconstruction', (20, 78), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (255, 200, 50), 1)
        cv2.putText(img, 'Clarity Test: ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 [ID: #PRO-9982X]', (20, 98), cv2.FONT_HERSHEY_SIMPLEX, 0.36, (255, 255, 255), 1)
        cv2.putText(img, 'Sub-Pixel Micro-Text: Catmull-Rom Keys Spline + Contrast-Adaptive Boost', (20, 116), cv2.FONT_HERSHEY_SIMPLEX, 0.30, (100, 255, 100), 1)

        cv2.rectangle(img, (10, 10), (w - 10, h - 10), (100, 100, 120), 1)
        return img

    def _capture_worker(self):
        target_interval = 1.0 / max(1.0, self.target_fps)
        next_frame_time = time.perf_counter()
        while self.is_running:
            now = time.perf_counter()
            if now < next_frame_time:
                rem = next_frame_time - now
                if rem > 0.002:
                    time.sleep(rem - 0.001)
                continue

            next_frame_time = now + target_interval

            if self.use_synthetic:
                frame = self._generate_synthetic_frame()
                ret = True
            else:
                ret, frame = self.cap.read()
                if (not ret or frame is None) and self.is_looping_video and self.cap:
                    self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    ret, frame = self.cap.read()

                if ret and frame is not None and self.stream_mode in ['cctv_campus', 'cctv_url']:
                    now_str = time.strftime('%Y-%m-%d %H:%M:%S') + f'.{int((time.time() % 1) * 1000):03d}'
                    cam_id = 'SEC-CAM-01 | LIVE SURVEILLANCE' if self.stream_mode == 'cctv_campus' else 'CAM-IP | LIVE CCTV'
                    label = f'[{cam_id} | {now_str} | REC]'
                    cv2.putText(frame, label, (12, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 0, 0), 2, cv2.LINE_AA)
                    cv2.putText(frame, label, (12, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 255, 255), 1, cv2.LINE_AA)

            if not ret or frame is None:
                time.sleep(0.002)
                continue

            cap_time = time.time()
            self.captured_count += 1

            with self.capture_lock:
                if self.new_raw_available:
                    self.discarded_count += 1
                self.raw_frame = frame
                self.raw_capture_time = cap_time
                self.new_raw_available = True
                self.frame_event.set()

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
                workload_multiplier=self.workload_multiplier,
                enhancement=self.enhancement_strength
            )
            t_end = time.time()

            end_to_end_lat = (t_end - cap_time) * 1000.0
            self.last_interp_latency = interp_latency
            self.last_end_to_end_latency = end_to_end_lat

            if self.adaptive_engine.adaptive_enabled:
                self.adaptive_engine.update_and_evaluate(self.frame_index, interp_latency)

            # Measure perceptual sharpness variance every 6 frames
            if self.frame_index % 6 == 0:
                try:
                    gray_up = cv2.cvtColor(upscaled_frame, cv2.COLOR_BGR2GRAY)
                    lap_up_raw = float(cv2.Laplacian(gray_up, cv2.CV_64F).var())
                    self.upscaled_sharpness = round(lap_up_raw, 1)

                    gray_raw = cv2.cvtColor(frame_to_upscale, cv2.COLOR_BGR2GRAY)
                    lap_raw = float(cv2.Laplacian(gray_raw, cv2.CV_64F).var())
                    self.raw_sharpness = round(lap_raw, 1)

                    self.raw_res_str = f'{frame_to_upscale.shape[1]}x{frame_to_upscale.shape[0]}'
                    self.upscaled_res_str = f'{upscaled_frame.shape[1]}x{upscaled_frame.shape[0]}'
                except Exception:
                    pass

            display_upscaled = upscaled_frame

            # Apply real-time lighting & sensor adjustments (Section 1.4 Objectives)
            if self.brightness != 0 or abs(self.contrast - 1.0) > 0.05:
                display_upscaled = cv2.convertScaleAbs(display_upscaled, alpha=self.contrast, beta=self.brightness)

            if self.low_light_boost:
                # Fast CLAHE on Luminance channel (Silva et al. [5])
                try:
                    ycrcb = cv2.cvtColor(display_upscaled, cv2.COLOR_BGR2YCrCb)
                    y, cr, cb = cv2.split(ycrcb)
                    y = self.clahe.apply(y)
                    display_upscaled = cv2.cvtColor(cv2.merge([y, cr, cb]), cv2.COLOR_YCrCb2BGR)
                except Exception:
                    pass

            if self.denoise:
                # Spatial Denoising (Dolwithayaku et al. [4])
                display_upscaled = cv2.GaussianBlur(display_upscaled, (3, 3), 0.75)

            if self.show_heatmap:
                try:
                    gray = cv2.cvtColor(upscaled_frame, cv2.COLOR_BGR2GRAY)
                    lap = cv2.Laplacian(gray, cv2.CV_8U, ksize=3)
                    heat = cv2.applyColorMap(lap * 3, cv2.COLORMAP_JET)
                    display_upscaled = cv2.addWeighted(upscaled_frame, 0.4, heat, 0.6, 0)
                except Exception:
                    pass

            # Record enhanced frame to live video recording
            with self.writer_lock:
                if self.is_running and display_upscaled is not None:
                    try:
                        dh, dw = display_upscaled.shape[:2]
                        if self.video_writer is None:
                            fourcc = cv2.VideoWriter_fourcc(*'mp4v')
                            self.video_writer = cv2.VideoWriter(self.recorded_video_path, fourcc, 30.0, (dw, dh))
                        if self.video_writer and self.video_writer.isOpened():
                            self.video_writer.write(display_upscaled)
                            self.recorded_frames_count += 1
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
        status['achieved_fps'] = status.get('rolling_avg_fps', status.get('current_fps', 120.0))
        status['discarded_frames'] = self.discarded_count
        status['processed_frames'] = self.processed_count
        status['captured_frames'] = self.captured_count
        status['interpolation_latency_ms'] = round(self.last_interp_latency, 2)
        status['end_to_end_latency_ms'] = round(self.last_end_to_end_latency, 2)
        status['scale_factor'] = self.scale_factor
        status['policy'] = self.policy
        status['num_threads'] = 1 if self.policy == 'SERIAL' else self.num_threads
        status['enhancement'] = self.enhancement_strength
        status['decisions'] = self.adaptive_engine.decision_history[-10:]

        status['raw_sharpness'] = self.raw_sharpness
        status['upscaled_sharpness'] = self.upscaled_sharpness
        sharpness_gain = round(((self.upscaled_sharpness - self.raw_sharpness) / max(0.1, self.raw_sharpness)) * 100.0, 1)
        status['sharpness_gain_pct'] = f'+{sharpness_gain}%' if sharpness_gain >= 0 else f'{sharpness_gain}%'

        try:
            rw, rh = [int(x) for x in self.raw_res_str.split('x')]
            raw_px = rw * rh
        except Exception:
            raw_px = 640 * 360

        try:
            uw, uh = [int(x) for x in self.upscaled_res_str.split('x')]
            up_px = uw * uh
        except Exception:
            up_px = 1920 * 1080

        status['raw_pixels'] = raw_px
        status['upscaled_pixels'] = up_px
        status['raw_res'] = self.raw_res_str
        status['upscaled_res'] = self.upscaled_res_str
        status['scale_factor'] = str(self.scale_factor)
        target_str = str(self.scale_factor).lower()
        if '1080' in target_str or 'fhd' in target_str:
            status['target_label'] = 'Full HD 1080p'
        elif '720' in target_str or 'hd' in target_str:
            status['target_label'] = 'HD 720p'
        elif '4k' in target_str or '2160' in target_str:
            status['target_label'] = 'Ultra HD 4K'
        else:
            status['target_label'] = f'{self.scale_factor}x'
        status['clarity_status'] = '💎 Crystal Clear AI (Active)'
        status['clarity_badge'] = f"💎 Crystal Clear ({status['target_label']})"
        status['stream_mode'] = self.stream_mode
        status['stream_source_label'] = self.stream_source_label

        # Enhanced Live Video Recording Metadata
        status['has_recorded_video'] = self.has_recorded_video
        status['recorded_frames'] = self.recorded_frames_count
        status['recorded_video_size_kb'] = round(os.path.getsize(self.recorded_video_path) / 1024.0, 1) if (self.has_recorded_video and os.path.exists(self.recorded_video_path)) else 0.0

        return status
