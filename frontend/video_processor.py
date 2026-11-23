import os
import time
import cv2
import numpy as np
from upscaler_engine import NativeUpscaler
from adaptive_engine import AdaptiveEngine

class VideoJobProcessor:
    def __init__(self):
        self.upscaler = NativeUpscaler()
        self.latest_frame_jpeg = None
        self.preview_frames = []
        self.is_processing = False

    def get_latest_frame(self):
        return self.latest_frame_jpeg

    def get_raw_preview_frame(self, index):
        if 0 <= index < len(self.preview_raw_frames):
            return self.preview_raw_frames[index]
        return None

    def get_preview_frame(self, index):
        if 0 <= index < len(self.preview_frames):
            return self.preview_frames[index]
        return None

    def process_file(self, input_path, output_path, scale_factor=2.0, method="BICUBIC",
                     policy="OPENMP_STATIC", num_threads=8, adaptive=False, target_fps=30.0,
                     enhancement=2.2, progress_callback=None):
        self.is_processing = True
        self.preview_frames = []
        self.preview_raw_frames = []
        self.latest_frame_jpeg = None

        try:
            cap = cv2.VideoCapture(input_path)
            if not cap.isOpened():
                raise IOError(f"Failed to open input video: {input_path}")

            src_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            src_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            if total_frames <= 0:
                total_frames = 90

            fps = cap.get(cv2.CAP_PROP_FPS)
            if fps <= 0.0 or np.isnan(fps):
                fps = 30.0

            dst_w = int(round(src_w * scale_factor))
            dst_h = int(round(src_h * scale_factor))

            fourcc = cv2.VideoWriter_fourcc(*"mp4v")
            out_writer = cv2.VideoWriter(output_path, fourcc, fps, (dst_w, dst_h))
            if not out_writer.isOpened():
                fourcc = cv2.VideoWriter_fourcc(*"XVID")
                out_writer = cv2.VideoWriter(output_path, fourcc, fps, (dst_w, dst_h))

            adaptive_engine = AdaptiveEngine(target_fps=target_fps, window_size=10, cooldown_frames=12)
            adaptive_engine.set_adaptive_enabled(adaptive)
            adaptive_engine.set_manual_method(method)

            processed_count = 0
            total_interp_latency = 0.0
            t_start_total = time.time()

            # Ensure optimal enhancement strength for crisp output
            enhancement_val = max(1.8, float(enhancement))

            while True:
                ret, frame = cap.read()
                if not ret or frame is None:
                    break

                processed_count += 1
                active_method = method
                if adaptive:
                    # In offline file export, never degrade to blocky NEAREST unless user explicitly requested NEAREST
                    cand = adaptive_engine.current_method
                    active_method = cand if (cand != 'NEAREST' or method == 'NEAREST') else 'BILINEAR'

                upscaled, interp_lat = self.upscaler.upscale(
                    frame,
                    scale_factor=scale_factor,
                    method=active_method,
                    policy=policy,
                    num_threads=num_threads,
                    chunk_size=16,
                    enhancement=enhancement_val
                )
                total_interp_latency += interp_lat

                if adaptive:
                    adaptive_engine.update_and_evaluate(processed_count, interp_lat)

                out_writer.write(upscaled)

                # Store latest frame preview JPEG at high visual quality (92%)
                _, up_jpg = cv2.imencode(".jpg", upscaled, [cv2.IMWRITE_JPEG_QUALITY, 92])
                jpg_bytes = up_jpg.tobytes()
                self.latest_frame_jpeg = jpg_bytes

                # Store both upscaled and original raw preview frames for side-by-side / split inspection
                if len(self.preview_frames) < 150:
                    _, raw_jpg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 90])
                    self.preview_frames.append(jpg_bytes)
                    self.preview_raw_frames.append(raw_jpg.tobytes())

                if progress_callback and (processed_count % 3 == 0 or processed_count == total_frames):
                    avg_lat = total_interp_latency / processed_count
                    progress_callback({
                        "current_frame": processed_count,
                        "total_frames": total_frames,
                        "progress_pct": min(99.0, round((processed_count / max(1, total_frames)) * 100.0, 1)),
                        "current_fps": round(1000.0 / max(0.01, interp_lat), 1),
                        "average_latency_ms": round(avg_lat, 2),
                        "current_method": active_method,
                        "switches_count": adaptive_engine.algorithm_switches,
                        "is_complete": False
                    })

            t_end_total = time.time()
            cap.release()
            out_writer.release()

            total_exec_time = t_end_total - t_start_total
            avg_latency = (total_interp_latency / max(1, processed_count))
            achieved_fps = (processed_count / total_exec_time) if total_exec_time > 0 else 0.0

            result_summary = {
                "input_path": input_path,
                "output_path": output_path,
                "current_frame": processed_count,
                "total_frames": processed_count,
                "progress_pct": 100.0,
                "current_fps": round(achieved_fps, 1),
                "src_resolution": f"{src_w}x{src_h}",
                "dst_resolution": f"{dst_w}x{dst_h}",
                "scale_factor": scale_factor,
                "original_fps": round(fps, 1),
                "achieved_fps": round(achieved_fps, 1),
                "average_latency_ms": round(avg_latency, 2),
                "total_time_sec": round(total_exec_time, 2),
                "method": method,
                "policy": policy,
                "threads": 1 if policy == "SERIAL" else num_threads,
                "adaptive": adaptive,
                "algorithm_switches": adaptive_engine.algorithm_switches,
                "decisions": adaptive_engine.decision_history,
                "preview_frame_count": len(self.preview_frames),
                "is_complete": True
            }

            if progress_callback:
                progress_callback(result_summary)

            return result_summary

        except Exception as e:
            err_summary = {
                "error": str(e),
                "is_complete": True,
                "progress_pct": 0.0
            }
            if progress_callback:
                progress_callback(err_summary)
            raise e
        finally:
            self.is_processing = False
