import os, sys, cv2, numpy as np
sys.path.append(r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\frontend')
from video_processor import VideoJobProcessor

# Create a sample 30-frame 320x240 video
test_in = r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\uploads\sample_test.mp4'
test_out = r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\outputs\upscaled_sample_test.mp4'

fourcc = cv2.VideoWriter_fourcc(*'mp4v')
writer = cv2.VideoWriter(test_in, fourcc, 30.0, (320, 240))
for i in range(30):
    f = np.zeros((240, 320, 3), dtype=np.uint8)
    f[:, :] = [i * 8, 120, 255 - i * 8]
    cv2.putText(f, f'Frame {i+1}', (50, 120), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)
    writer.write(f)
writer.release()
print('Generated input video:', test_in, os.path.getsize(test_in), 'bytes')

# Process with VideoJobProcessor
vp = VideoJobProcessor()
res = vp.process_file(
    input_path=test_in,
    output_path=test_out,
    scale_factor=2.0,
    method='BICUBIC',
    policy='OPENMP_STATIC',
    num_threads=4,
    adaptive=True,
    target_fps=30.0,
    progress_callback=lambda p: print('Progress:', p['current_frame'], '/', p['total_frames'], 'FPS:', p['current_fps']) if p['current_frame'] % 10 == 0 else None
)

print('\n=== VIDEO RECONSTRUCTION VERIFICATION ===')
print('Output Path:', res['output_path'], 'Size:', os.path.getsize(res['output_path']), 'bytes')
print('Input Resolution:', res['src_resolution'])
print('Output Resolution:', res['dst_resolution'])
print('Total Frames Processed:', res['total_frames'])
print('Average Latency:', res['average_latency_ms'], 'ms')
print('Achieved FPS:', res['achieved_fps'], 'FPS')

out_cap = cv2.VideoCapture(test_out)
assert out_cap.isOpened(), 'Output video failed to open'
out_w = int(out_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
out_h = int(out_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
out_count = int(out_cap.get(cv2.CAP_PROP_FRAME_COUNT))
out_cap.release()

print(f'Verified Reconstructed Video: {out_w}x{out_h}, Frame Count: {out_count}')
assert out_w == 640 and out_h == 480, 'Output dimensions mismatch'
assert out_count == 30, 'Frame count mismatch (frames were lost)'
print('SUCCESS! Uploaded video mode 100% verified!')
