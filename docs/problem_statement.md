# Problem Statement & Research Motivation

## 1. Introduction & Context
High-resolution displays (1080p, 4K, and beyond) are ubiquitous in modern consumer electronics, video conferencing, and broadcast media. However, network transmission bandwidth, remote edge devices, and sensor limitations frequently restrict captured video streams to standard or low definitions (360p, 480p). Video upscaling—the reconstruction of high-resolution video frames from lower-resolution inputs—is a fundamental computational vision requirement.

## 2. Research Problem
Conventional video processing pipelines typically commit to a fixed spatial interpolation filter:
1. **Nearest Neighbour**: Computationally lightweight ($\mathcal{O}(1)$), but exhibits severe aliasing, pixelation, and block artifacts.
2. **Bilinear Interpolation**: Moderate arithmetic intensity ($\mathcal{O}(4)$ per pixel), but introduces high-frequency blurring and loss of edge contrast.
3. **Bicubic Spline Interpolation**: High fidelity ($\mathcal{O}(16)$ per pixel, Catmull-Rom cubic kernel), producing crisp edges, but incurring high computational latency.

In real-world live video applications (e.g., webcam conferencing, robotic teleoperation), computing resources fluctuate due to system background tasks, thermal CPU throttling, or varying frame complexity. Under a fixed high-quality method (such as Bicubic), a sudden processing delay causes the frame time to exceed the target budget ({\text{budget}} = 1000 / \text{FPS}$ ms), resulting in frame backlogs, high latency, dropped frames, and video stutter.

## 3. The Research Gap
Existing commercial and academic video scaling solutions either:
- Rely on heavy GPU-dependent deep neural networks (e.g., SRCNN, Real-ESRGAN) that cannot run in real time on general-purpose CPUs without dedicated NVIDIA hardware.
- Commit to a static interpolation method that fails to adapt when computational headroom degrades.

## 4. Proposed Solution & Objectives
This project proposes an **Adaptive Real-Time Video Upscaling System** using multi-core CPU parallelisation with OpenMP.
Key objectives:
1. **Parallel Execution**: Leverage multi-core CPU architectures (Intel 12th Gen Core i5-12450H with 12 hardware threads) to parallelise pixel and scanline interpolation using OpenMP (static, dynamic, and guided scheduling).
2. **Quality-Aware Adaptivity**: Implement a closed-loop feedback engine that continuously monitors frame processing latency against the target frame budget.
3. **Dynamic Quality Selection**: When overload occurs ({\text{latency}} > T_{\text{budget}}$), deterministically downgrade to faster interpolation methods; when sustained headroom is detected ({\text{latency}} \ll T_{\text{budget}}$), upgrade back to high-fidelity bicubic interpolation.
4. **Low-Latency Asynchronous Architecture**: Separate frame capture, interpolation processing, and display pipelines using bounded 1-slot buffers to eliminate frame accumulation and minimize capture-to-display latency.
