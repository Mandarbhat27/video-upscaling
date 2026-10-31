# Adaptive Real-Time Video Upscaling
## Quality-Aware Parallel Video Processing Using OpenMP

An academic-grade, low-latency video upscaling system that dynamically balances perceptual visual quality against computational latency budgets using multi-core CPU parallelisation.

---

## Key Highlights & System Contributions

- **Genuine C++17 OpenMP Parallel Engine**: Pixel and scanline interpolation parallelised with OpenMP 5.2 across up to 12 CPU threads (static, dynamic, and guided scheduling).
- **Feedback-Driven Adaptive Quality Engine**: Continuously monitors per-frame processing latency against the target frame budget (T_budget = 1000 / FPS ms). Automatically switches between **Bicubic**, **Bilinear**, and **Nearest Neighbour** algorithms using hysteresis thresholds and cooldown periods to prevent rapid thrashing.
- **Dual Input Modes**:
  1. **Live Webcam Mode**: Low-latency asynchronous pipeline with 1-slot buffer and stale frame dropping to maintain sub-35ms end-to-end responsiveness (includes automatic synthetic test pattern fallback if hardware camera is unattached).
  2. **Uploaded Video Mode**: Complete frame extraction, strict frame order preservation, OpenMP upscaling, and output MP4 reconstruction with download capability.
- **Controlled Adaptivity Stress Controls**: Repeatable workload stress multiplier (1x Normal, 2x Increased, 4x Heavy) allowing reproducible demonstrations of adaptive downgrading and recovery under load.
- **Empirical Benchmarking**: Measured speedup up to **4.06x** on Intel Core i5-12450H CPU. Real CSV and JSON exports.

---

## Technology Stack

- **Computation Core**: C++17 (`adaptive_core.cpp`, `adaptive_core.dll`), OpenMP 5.2, MinGW-w64 GCC 16.2.0.
- **Computer Vision**: OpenCV 4.5.5-dev for video capture, frame manipulation, and video encoding.
- **Orchestration**: Python 3.10 with Flask 3.1.3 and ctypes native bindings.
- **Frontend Dashboard**: HTML5, Bootstrap 5, Chart.js for real-time telemetry graphs.

---

## Quickstart & Execution Instructions

### 1. Requirements
Ensure Python 3.10+ is installed:
```bash
pip install -r requirements.txt
```

### 2. Launch the Live Dashboard
To run the web dashboard on Windows:
```bash
python frontend/app.py
```
Open your browser and navigate to:
```
http://127.0.0.1:5000
```

### 3. Run the Automated Verification Suite
Run the comprehensive test suite verifying the adaptive engine, video reconstruction, and low-latency webcam pipeline:
```bash
python tests/run_all_tests.py
```

### 4. Run Benchmarks & Export Data
To execute the automated benchmark suite:
```bash
python -c "import sys; sys.path.append('frontend'); from benchmark_runner import BenchmarkRunner; r = BenchmarkRunner(); recs = r.run_full_benchmark(); r.export_results(recs)"
```
Results are saved to `results/benchmark_results.csv` and `results/benchmark_results.json`.

---

## Demonstration Guide for Project Presentation

1. **Start the Application**: Run `python frontend/app.py` and open `http://127.0.0.1:5000`.
2. **Start Live Camera Feed**: Click **Start Camera** in Webcam Mode. The dual-pane view displays original frames on the left and 2x upscaled frames on the right.
3. **Verify Baseline OpenMP Performance**:
   - Select **Bicubic** method, **OpenMP Static**, and **4 Threads**.
   - Observe processing latency (~15ms) well below the 33.3ms budget at 30 FPS.
4. **Trigger Controlled Overload**:
   - Click the **2x Load** or **4x Heavy** stress button.
   - Observe processing latency spike above the budget line on the real-time Chart.js graph.
   - Watch the **Adaptive Status** badge turn red (*Processing Overload*) and observe the engine automatically downgrade from **Bicubic -> Bilinear -> Nearest**.
   - Inspect the **Adaptive Decision History** table recording the frame number, latency, and reason.
5. **Demonstrate Quality Recovery**:
   - Click **Normal** stress.
   - Watch latency return below budget and the engine smoothly upgrade back to **Bilinear** and **Bicubic**.
6. **Uploaded Video Processing**:
   - Switch to **Uploaded Video Mode**.
   - Upload a test video (e.g. `uploads/sample_test.mp4`), configure scale/threads, and click **Start Processing**.
   - Watch the progress bar, preview the output video, and click **Download Reconstructed Video**.
7. **Inspect Empirical Benchmark Curves**:
   - Navigate to `/results` to view the Speedup vs. Thread Count curves and parallel efficiency metrics.
