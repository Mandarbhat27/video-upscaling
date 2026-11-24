import os
import sys
import json
import time
import threading
import base64
import cv2
import numpy as np
from flask import Flask, render_template, Response, request, jsonify, send_file
from werkzeug.utils import secure_filename

curr_dir = os.path.dirname(os.path.abspath(__file__))
if curr_dir not in sys.path:
    sys.path.append(curr_dir)

from webcam_streamer import WebcamStreamer
from video_processor import VideoJobProcessor
from benchmark_runner import BenchmarkRunner
from upscaler_engine import NativeUpscaler

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = os.path.abspath(os.path.join(curr_dir, '..', 'uploads'))
app.config['OUTPUT_FOLDER'] = os.path.abspath(os.path.join(curr_dir, '..', 'outputs'))
app.config['RESULTS_FOLDER'] = os.path.abspath(os.path.join(curr_dir, '..', 'results'))

os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
os.makedirs(app.config['OUTPUT_FOLDER'], exist_ok=True)
os.makedirs(app.config['RESULTS_FOLDER'], exist_ok=True)

webcam_worker = WebcamStreamer()
video_processor = VideoJobProcessor()
benchmark_runner = BenchmarkRunner()
photo_upscaler = NativeUpscaler()

video_job_status = {
    'current_frame': 0,
    'total_frames': 0,
    'progress_pct': 0,
    'current_fps': 0,
    'average_latency_ms': 0,
    'current_method': 'BILINEAR',
    'is_complete': False,
    'output_file': None,
    'error': None
}

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/results')
def results():
    return render_template('results.html')

# --- CAMERA / WEBCAM & CCTV ENDPOINTS ---
@app.route('/api/camera/start', methods=['POST'])
def start_camera():
    data = request.get_json() or {}
    mode = data.get('mode', 'webcam')
    stream_url = data.get('stream_url', '')
    success = webcam_worker.start(mode=mode, stream_url=stream_url)
    return jsonify({
        'status': 'ok' if success else 'error',
        'mode': mode,
        'source_label': webcam_worker.stream_source_label
    })

@app.route('/api/camera/stop', methods=['POST'])
def stop_camera():
    webcam_worker.stop()
    return jsonify({'status': 'ok'})

@app.route('/api/control', methods=['POST'])
def update_control():
    data = request.get_json() or {}
    webcam_worker.set_controls(
        scale=data.get('scale'),
        method=data.get('method'),
        policy=data.get('policy'),
        threads=data.get('threads'),
        target_fps=data.get('target_fps'),
        adaptive=data.get('adaptive'),
        workload=data.get('workload'),
        show_heatmap=data.get('show_heatmap'),
        enhancement=data.get('enhancement'),
        brightness=data.get('brightness'),
        contrast=data.get('contrast'),
        low_light_boost=data.get('low_light_boost'),
        denoise=data.get('denoise')
    )
    return jsonify({'status': 'ok'})

@app.route('/api/status')
def get_status():
    return jsonify(webcam_worker.get_metrics())

# Single frame endpoints for universal polling fallback
@app.route('/api/camera/frame/raw')
def get_single_raw_frame():
    frame = webcam_worker.get_raw_jpeg()
    if frame:
        return Response(frame, mimetype='image/jpeg', headers={'Cache-Control': 'no-cache, no-store'})
    return ('', 204)

@app.route('/api/camera/frame/upscaled')
def get_single_upscaled_frame():
    frame = webcam_worker.get_upscaled_jpeg()
    if frame:
        return Response(frame, mimetype='image/jpeg', headers={'Cache-Control': 'no-cache, no-store'})
    return ('', 204)

def gen_mjpeg_raw():
    waited = 0
    while not webcam_worker.is_running and waited < 30:
        time.sleep(0.05)
        waited += 1

    while webcam_worker.is_running:
        frame_bytes = webcam_worker.get_raw_jpeg()
        if frame_bytes:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n'
                   b'Content-Length: ' + str(len(frame_bytes)).encode('ascii') + b'\r\n\r\n' +
                   frame_bytes + b'\r\n')
        time.sleep(max(0.005, 1.0 / (webcam_worker.target_fps * 1.15)))

def gen_mjpeg_upscaled():
    waited = 0
    while not webcam_worker.is_running and waited < 30:
        time.sleep(0.05)
        waited += 1

    while webcam_worker.is_running:
        frame_bytes = webcam_worker.get_upscaled_jpeg()
        if frame_bytes:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n'
                   b'Content-Length: ' + str(len(frame_bytes)).encode('ascii') + b'\r\n\r\n' +
                   frame_bytes + b'\r\n')
        time.sleep(max(0.005, 1.0 / (webcam_worker.target_fps * 1.15)))

@app.route('/video_feed/raw')
def video_feed_raw():
    resp = Response(gen_mjpeg_raw(), mimetype='multipart/x-mixed-replace; boundary=frame')
    resp.headers['Cache-Control'] = 'no-cache, no-store, must-revalidate'
    resp.headers['Pragma'] = 'no-cache'
    resp.headers['Expires'] = '0'
    return resp

@app.route('/video_feed/upscaled')
def video_feed_upscaled():
    resp = Response(gen_mjpeg_upscaled(), mimetype='multipart/x-mixed-replace; boundary=frame')
    resp.headers['Cache-Control'] = 'no-cache, no-store, must-revalidate'
    resp.headers['Pragma'] = 'no-cache'
    resp.headers['Expires'] = '0'
    return resp

# --- UPLOADED VIDEO ENDPOINTS ---
@app.route('/api/upload_video', methods=['POST'])
def upload_video():
    global video_job_status

    use_sample = request.form.get('use_sample') in ['true', 'True', '1', True]
    if use_sample:
        input_path = os.path.join(app.config['UPLOAD_FOLDER'], 'sample_demo.mp4')
        if not os.path.exists(input_path):
            return jsonify({'status': 'error', 'message': 'Sample video not found on server'}), 404
        filename = 'sample_demo.mp4'
    else:
        if 'video' not in request.files:
            return jsonify({'status': 'error', 'message': 'No video file provided'}), 400
        file = request.files['video']
        if file.filename == '':
            return jsonify({'status': 'error', 'message': 'No selected file'}), 400
        filename = secure_filename(file.filename)
        input_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(input_path)

    scale = float(request.form.get('scale', 2.0))
    policy = request.form.get('policy', 'OPENMP_STATIC')
    method = request.form.get('method', 'BICUBIC')
    threads = int(request.form.get('threads', 8))
    adaptive = request.form.get('adaptive') in ['true', 'True', '1', True]
    enhancement = float(request.form.get('enhancement', 3.5))

    output_filename = f'upscaled_{filename}'
    output_path = os.path.join(app.config['OUTPUT_FOLDER'], output_filename)
    
    video_job_status = {
        'current_frame': 0,
        'total_frames': 0,
        'progress_pct': 0.0,
        'current_fps': 0,
        'average_latency_ms': 0,
        'current_method': method,
        'is_complete': False,
        'output_file': output_path,
        'error': None
    }

    def progress_callback(prog):
        global video_job_status
        video_job_status.update(prog)

    def worker():
        try:
            video_processor.process_file(
                input_path=input_path,
                output_path=output_path,
                scale_factor=scale,
                method=method,
                policy=policy,
                num_threads=threads,
                adaptive=adaptive,
                enhancement=enhancement,
                progress_callback=progress_callback
            )
        except Exception as e:
            video_job_status['error'] = str(e)
            video_job_status['is_complete'] = True

    t = threading.Thread(target=worker, daemon=True)
    t.start()

    return jsonify({'status': 'started', 'filename': filename})

@app.route('/api/video_progress')
def get_video_progress():
    return jsonify(video_job_status)

@app.route('/api/video_current_frame')
def get_video_current_frame():
    frame = video_processor.get_latest_frame()
    if frame:
        return Response(frame, mimetype='image/jpeg', headers={'Cache-Control': 'no-cache, no-store'})
    return ('', 204)

@app.route('/api/video_raw_preview_frame/<int:idx>')
def get_video_raw_preview_frame(idx):
    frame = video_processor.get_raw_preview_frame(idx)
    if frame:
        return Response(frame, mimetype='image/jpeg', headers={'Cache-Control': 'public, max-age=3600'})
    return ('', 404)

@app.route('/api/video_preview_frame/<int:idx>')
def get_video_preview_frame(idx):
    frame = video_processor.get_preview_frame(idx)
    if frame:
        return Response(frame, mimetype='image/jpeg', headers={'Cache-Control': 'public, max-age=3600'})
    return ('', 404)


# --- PHOTO / SNAPSHOT UPSCALE ENDPOINTS ---
@app.route('/api/upscale_photo', methods=['POST'])
def upscale_photo():
    use_sample = request.form.get('use_sample') in ['true', 'True', '1', True]
    from_webcam = request.form.get('from_webcam') in ['true', 'True', '1', True]
    
    img = None
    filename = 'photo.jpg'
    
    if from_webcam:
        if webcam_worker.raw_frame is not None:
            img = webcam_worker.raw_frame.copy()
            filename = f'webcam_snapshot_{int(time.time())}.jpg'
        else:
            img = webcam_worker._generate_synthetic_frame()
            filename = f'webcam_snapshot_{int(time.time())}.jpg'
    elif use_sample:
        sample_path = os.path.join(app.config['UPLOAD_FOLDER'], 'sample_interview_snapshot.jpg')
        if os.path.exists(sample_path):
            img = cv2.imread(sample_path)
            filename = 'sample_interview_snapshot.jpg'
        else:
            return jsonify({'status': 'error', 'message': 'Sample photo not found'}), 404
    else:
        if 'photo' not in request.files:
            return jsonify({'status': 'error', 'message': 'No photo file provided'}), 400
        file = request.files['photo']
        if file.filename == '':
            return jsonify({'status': 'error', 'message': 'No selected file'}), 400
        filename = secure_filename(file.filename)
        save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(save_path)
        img = cv2.imread(save_path)

    if img is None or img.size == 0:
        return jsonify({'status': 'error', 'message': 'Could not decode image'}), 400

    scale = float(request.form.get('scale', 2.0))
    policy = request.form.get('policy', 'OPENMP_STATIC')
    method = request.form.get('method', 'BICUBIC')
    threads = int(request.form.get('threads', 8))
    enhancement = float(request.form.get('enhancement', 3.5))

    src_h, src_w = img.shape[:2]
    src_pixels = src_w * src_h

    # Measure edge sharpness of original using Laplacian Variance
    gray_src = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    src_sharpness = round(float(cv2.Laplacian(gray_src, cv2.CV_64F).var()), 1)

    # Upscale and enhance with C++ OpenMP native library
    upscaled, latency_ms = photo_upscaler.upscale(
        img,
        scale_factor=scale,
        method=method,
        policy=policy,
        num_threads=threads,
        chunk_size=16,
        enhancement=enhancement
    )

    # Apply lighting & sensor adjustments (Section 1.4 Objectives)
    brightness = int(request.form.get('brightness', 0))
    contrast = float(request.form.get('contrast', 1.0))
    low_light_boost = request.form.get('low_light_boost') in ['true', 'True', '1', True]
    denoise = request.form.get('denoise') in ['true', 'True', '1', True]

    if brightness != 0 or abs(contrast - 1.0) > 0.05:
        upscaled = cv2.convertScaleAbs(upscaled, alpha=contrast, beta=brightness)
    if low_light_boost:
        clahe = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8))
        ycrcb = cv2.cvtColor(upscaled, cv2.COLOR_BGR2YCrCb)
        y, cr, cb = cv2.split(ycrcb)
        y = clahe.apply(y)
        upscaled = cv2.cvtColor(cv2.merge([y, cr, cb]), cv2.COLOR_YCrCb2BGR)
    if denoise:
        upscaled = cv2.GaussianBlur(upscaled, (3, 3), 0.75)

    dst_h, dst_w = upscaled.shape[:2]
    dst_pixels = dst_w * dst_h
    pixel_gain_pct = round(((dst_pixels - src_pixels) / src_pixels) * 100)

    # Measure edge sharpness of upscaled using Laplacian Variance
    gray_dst = cv2.cvtColor(upscaled, cv2.COLOR_BGR2GRAY)
    dst_sharpness_raw = float(cv2.Laplacian(gray_dst, cv2.CV_64F).var())
    
    # Scale-normalized perceptual clarity (accounting for 2x/4x grid expansion)
    norm_factor = scale * scale
    dst_sharpness_norm = dst_sharpness_raw * norm_factor
    sharpness_gain_pct = round(((dst_sharpness_norm - src_sharpness) / max(0.1, src_sharpness)) * 100, 1)
    
    dst_sharpness = round(dst_sharpness_norm, 1)

    # Save output photo
    out_filename = f'upscaled_{filename}'
    out_path = os.path.join(app.config['OUTPUT_FOLDER'], out_filename)
    if out_path.lower().endswith('.png'):
        cv2.imwrite(out_path, upscaled, [cv2.IMWRITE_PNG_COMPRESSION, 3])
    else:
        cv2.imwrite(out_path, upscaled, [cv2.IMWRITE_JPEG_QUALITY, 95])

    # Base64 data URLs for instant rendering in browser
    _, src_buf = cv2.imencode('.jpg', img, [cv2.IMWRITE_JPEG_QUALITY, 85])
    _, dst_buf = cv2.imencode('.jpg', upscaled, [cv2.IMWRITE_JPEG_QUALITY, 90])
    src_b64 = 'data:image/jpeg;base64,' + base64.b64encode(src_buf.tobytes()).decode('ascii')
    dst_b64 = 'data:image/jpeg;base64,' + base64.b64encode(dst_buf.tobytes()).decode('ascii')

    return jsonify({
        'status': 'ok',
        'src_image_url': src_b64,
        'dst_image_url': dst_b64,
        'download_url': f'/api/download_photo?file={out_filename}',
        'filename': out_filename,
        'src_resolution': f'{src_w}x{src_h}',
        'dst_resolution': f'{dst_w}x{dst_h}',
        'src_pixels': src_pixels,
        'dst_pixels': dst_pixels,
        'pixel_gain_pct': f'+{pixel_gain_pct}%',
        'latency_ms': round(latency_ms, 2),
        'src_sharpness': src_sharpness,
        'dst_sharpness': dst_sharpness,
        'sharpness_gain_pct': f'+{sharpness_gain_pct}%',
        'method': method,
        'policy': policy,
        'threads': 1 if policy == 'SERIAL' else threads,
        'enhancement': enhancement
    })

@app.route('/api/download_photo')
def download_photo():
    fn = request.args.get('file', '')
    if not fn:
        return jsonify({'error': 'No file specified'}), 400
    safe_fn = secure_filename(fn)
    fp = os.path.join(app.config['OUTPUT_FOLDER'], safe_fn)
    if os.path.exists(fp):
        return send_file(fp, as_attachment=True, download_name=safe_fn)
    return jsonify({'error': 'File not found'}), 404

@app.route('/api/download_video')
def download_video():
    out_file = video_job_status.get('output_file')
    if out_file and os.path.exists(out_file):
        return send_file(out_file, as_attachment=True, download_name=os.path.basename(out_file))
    return jsonify({'error': 'File not found'}), 404

# --- BENCHMARK ENDPOINTS ---
@app.route('/api/run_benchmark', methods=['POST'])
def run_benchmark():
    records = benchmark_runner.run_full_benchmark(scale_factors=[2.0], thread_list=[1, 2, 4, 8, 12], iterations=3)
    benchmark_runner.export_results(records, output_dir=app.config['RESULTS_FOLDER'])
    return jsonify({'status': 'ok', 'record_count': len(records)})

@app.route('/api/benchmark_data')
def get_benchmark_data():
    json_path = os.path.join(app.config['RESULTS_FOLDER'], 'benchmark_results.json')
    if os.path.exists(json_path):
        with open(json_path, 'r') as f:
            data = json.load(f)
        return jsonify(data)
    return jsonify([])

@app.route('/api/download_csv')
def download_csv():
    csv_path = os.path.join(app.config['RESULTS_FOLDER'], 'benchmark_results.csv')
    if os.path.exists(csv_path):
        return send_file(csv_path, as_attachment=True, download_name='benchmark_results.csv')
    return jsonify({'error': 'File not found'}), 404

if __name__ == '__main__':
    print("=" * 60)
    print("  Adaptive Video Upscaling Server (OpenMP + Flask)")
    print("  URL: http://127.0.0.1:5000")
    print("=" * 60)
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
