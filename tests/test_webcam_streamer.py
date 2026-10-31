import sys, time
sys.path.append(r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\frontend')
from webcam_streamer import WebcamStreamer

print('=== TESTING LOW-LATENCY WEBCAM PIPELINE ===')
streamer = WebcamStreamer()
streamer.start()
time.sleep(1.5)

raw_jpg = streamer.get_raw_jpeg()
up_jpg = streamer.get_upscaled_jpeg()

assert raw_jpg is not None and len(raw_jpg) > 0, 'Raw JPEG buffer is empty'
assert up_jpg is not None and len(up_jpg) > 0, 'Upscaled JPEG buffer is empty'
assert raw_jpg[:2] == b'\xff\xd8', 'Invalid raw JPEG header'
assert up_jpg[:2] == b'\xff\xd8', 'Invalid upscaled JPEG header'

metrics = streamer.get_metrics()
print('Captured Frames:', metrics['captured_frames'])
print('Processed Frames:', metrics['processed_frames'])
print('Discarded Frames:', metrics['discarded_frames'])
print('Interpolation Latency:', metrics['interpolation_latency_ms'], 'ms')
print('End-to-End Latency:', metrics['end_to_end_latency_ms'], 'ms')
print('Active Method:', metrics['current_method'])
print('State Message:', metrics['state_message'])

# Test workload stress increase
print('\nApplying HIGH workload stress...')
streamer.set_controls(workload='HIGH')
time.sleep(1.0)
metrics_stressed = streamer.get_metrics()
print('Stressed Interp Latency:', metrics_stressed['interpolation_latency_ms'], 'ms')
print('Stressed Workload Level:', metrics_stressed['workload_level'])

streamer.stop()
print('\n=== WEBCAM PIPELINE TEST PASSED! ===')
