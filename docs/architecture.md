# System Architecture & Low-Latency Pipeline

## 1. High-Level Architecture Overview
The system follows a modular decoupled architecture:
1. **Computational Core (C++17 + OpenMP)**: High-performance image interpolation algorithms compiled into a native shared library (daptive_core.dll). Employs #pragma omp parallel for with configurable scheduling policies (static, dynamic, guided).
2. **Orchestration & Asynchronous Pipeline (Python Flask + OpenCV)**: Manages video capture, thread synchronisation, HTTP MJPEG streaming, file uploads, and telemetry API.
3. **Frontend Dashboard (HTML5, Bootstrap, Chart.js)**: Displays real-time side-by-side video feeds, performance meters, interactive stress controls, and live Chart.js graphs.

## 2. Low-Latency Pipeline Design
To prevent processing delays from causing unbounded buffering or stale frame display, the pipeline decouples execution stages:

`
[Webcam / Video Source]
         │
         ▼
[Thread 1: Capture Worker]
         │ (1-Slot Overwrite Buffer)
         ▼
[Thread 2: OpenMP Processing Worker] ──► [Adaptive Feedback Engine]
         │                                          │
         ▼                                          ▼
[Thread 3: JPEG Encoder & Streamer]      [Dynamic Quality Adjustment]
         │
         ▼
[Web Dashboard / Browser UI]
`

### Stale Frame Dropping Mechanism (Webcam Mode)
- The capture thread continuously reads frames from cv::VideoCapture.
- If a new frame arrives before the processing thread finishes the previous frame, the older unconsumed frame is discarded, and the discarded_frames counter is incremented.
- This guarantees that the processing thread always operates on the **freshest available frame**, preventing lag accumulation and maintaining sub-35ms end-to-end latency.

### Strict Order Preservation (Uploaded Video Mode)
- In contrast to live webcam mode, uploaded video mode processes every single frame sequentially without dropping.
- The output video is reconstructed at the original framerate using OpenCV VideoWriter.
