import subprocess
import sys
import time

tests = [
    ('Adaptive Engine Switching & Stability', [sys.executable, r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\tests\test_adaptive_engine.py']),
    ('Uploaded Video Processing & Reconstruction', [sys.executable, r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\tests\test_video_pipeline.py']),
    ('Low-Latency Webcam Streaming Pipeline', [sys.executable, r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\tests\test_webcam_streamer.py']),
]

print('======================================================================')
print('   ADAPTIVE VIDEO UPSCALING - COMPREHENSIVE VERIFICATION SUITE')
print('======================================================================')

results = []
for name, cmd in tests:
    print('\n>> RUNNING: ' + name + '...')
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True)
    duration = time.time() - t0

    if res.returncode == 0:
        print(f'   [PASS] in {duration:.2f}s')
        results.append((name, 'PASSED', f'{duration:.2f}s'))
    else:
        print(f'   [FAIL] in {duration:.2f}s')
        print(res.stdout)
        print(res.stderr)
        results.append((name, 'FAILED', f'{duration:.2f}s'))

print('\n======================================================================')
print('                         FINAL TEST SUMMARY')
print('======================================================================')
print('{:<46} | {:<8} | {:<8}'.format('Test Name', 'Status', 'Duration'))
print('-' * 70)
for name, status, dur in results:
    print('{:<46} | {:<8} | {:<8}'.format(name, status, dur))
print('======================================================================')

all_passed = all(s == 'PASSED' for _, s, _ in results)
sys.exit(0 if all_passed else 1)
