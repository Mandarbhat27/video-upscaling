let cameraActive = false;
let metricsInterval = null;
let currentMode = 'webcam';
let cameraPollInterval = null;

let isHeatmapActive = false;
let isNativeViewActive = false;
let isZoomLoupeActive = true;
let hoverNormX = 0.5;
let hoverNormY = 0.5;

document.addEventListener('DOMContentLoaded', () => {
    if (typeof initCharts === 'function') {
        initCharts();
    }
    setupZoomListeners();
});

function switchMode(mode) {
    currentMode = mode;
    const webcamSec = document.getElementById('webcam-section');
    const videoSec = document.getElementById('video-section');
    const photoSec = document.getElementById('photo-section');
    const webcamBtn = document.getElementById('mode-webcam-btn');
    const videoBtn = document.getElementById('mode-video-btn');
    const photoBtn = document.getElementById('mode-photo-btn');

    [webcamSec, videoSec, photoSec].forEach(sec => { if (sec) sec.style.display = 'none'; });
    [webcamBtn, videoBtn, photoBtn].forEach(btn => {
        if (btn) {
            btn.classList.remove('btn-primary', 'active');
            btn.classList.add('btn-outline-secondary');
        }
    });

    if (mode === 'webcam') {
        if (webcamSec) webcamSec.style.display = 'block';
        if (webcamBtn) { webcamBtn.classList.add('btn-primary', 'active'); webcamBtn.classList.remove('btn-outline-secondary'); }
    } else if (mode === 'video') {
        if (videoSec) videoSec.style.display = 'block';
        if (videoBtn) { videoBtn.classList.add('btn-primary', 'active'); videoBtn.classList.remove('btn-outline-secondary'); }
    } else if (mode === 'photo') {
        if (photoSec) photoSec.style.display = 'block';
        if (photoBtn) { photoBtn.classList.add('btn-primary', 'active'); photoBtn.classList.remove('btn-outline-secondary'); }
    }
}

function toggleCamera() {
    const btn = document.getElementById('cam-toggle-btn');
    const rawImg = document.getElementById('raw-video-img');
    const upscaledImg = document.getElementById('upscaled-video-img');
    const rawPlaceholder = document.getElementById('raw-placeholder');
    const upscaledPlaceholder = document.getElementById('upscaled-placeholder');

    if (!cameraActive) {
        btn.disabled = true;
        btn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Starting Engine...';

        fetch('/api/camera/start', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                btn.disabled = false;
                if (data.status === 'ok') {
                    cameraActive = true;
                    btn.classList.remove('btn-success');
                    btn.classList.add('btn-danger');
                    btn.innerHTML = '<i class="bi bi-stop-fill me-1"></i> Stop Camera';

                    rawPlaceholder.style.display = 'none';
                    upscaledPlaceholder.style.display = 'none';
                    rawImg.style.display = 'block';
                    upscaledImg.style.display = 'block';

                    const ts = Date.now();
                    rawImg.src = '/video_feed/raw?ts=' + ts;
                    upscaledImg.src = '/video_feed/upscaled?ts=' + ts;

                    rawImg.onerror = () => {
                        console.warn('MJPEG dropped, activating fast polling fallback');
                        startCameraPollingFallback();
                    };

                    if (!metricsInterval) {
                        metricsInterval = setInterval(pollMetrics, 300);
                    }
                } else {
                    alert('Camera could not be started: ' + (data.message || 'Check terminal'));
                    btn.classList.remove('btn-danger');
                    btn.classList.add('btn-success');
                    btn.innerHTML = '<i class="bi bi-play-fill me-1"></i> Start Camera';
                }
            })
            .catch(err => {
                btn.disabled = false;
                btn.classList.remove('btn-danger');
                btn.classList.add('btn-success');
                btn.innerHTML = '<i class="bi bi-play-fill me-1"></i> Start Camera';
                alert('Connection error. Ensure "python app.py" is running in your terminal.');
                console.error(err);
            });
    } else {
        btn.disabled = true;
        btn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Stopping...';

        fetch('/api/camera/stop', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                btn.disabled = false;
                cameraActive = false;
                btn.classList.remove('btn-danger');
                btn.classList.add('btn-success');
                btn.innerHTML = '<i class="bi bi-play-fill me-1"></i> Start Camera';

                rawImg.style.display = 'none';
                upscaledImg.style.display = 'none';
                rawImg.src = '';
                upscaledImg.src = '';
                rawPlaceholder.style.display = 'block';
                upscaledPlaceholder.style.display = 'block';

                if (metricsInterval) {
                    clearInterval(metricsInterval);
                    metricsInterval = null;
                }
                if (cameraPollInterval) {
                    clearInterval(cameraPollInterval);
                    cameraPollInterval = null;
                }
            })
            .catch(err => {
                btn.disabled = false;
                console.error(err);
            });
    }
}

function startCameraPollingFallback() {
    if (cameraPollInterval) return;
    const rawImg = document.getElementById('raw-video-img');
    const upscaledImg = document.getElementById('upscaled-video-img');

    cameraPollInterval = setInterval(() => {
        if (!cameraActive) return;
        const ts = Date.now();
        rawImg.src = '/api/camera/frame/raw?ts=' + ts;
        upscaledImg.src = '/api/camera/frame/upscaled?ts=' + ts;
    }, 40);
}

function updateControls() {
    const scale = document.getElementById('ctrl-scale').value;
    const policy = document.getElementById('ctrl-policy').value;
    const threads = document.getElementById('ctrl-threads').value;
    const targetFps = document.getElementById('ctrl-target-fps').value;
    const adaptive = document.getElementById('ctrl-adaptive').checked;

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            scale: scale,
            policy: policy,
            threads: threads,
            target_fps: targetFps,
            adaptive: adaptive,
            show_heatmap: isHeatmapActive
        })
    }).catch(err => console.error(err));
}

function toggleHeatmap() {
    isHeatmapActive = !isHeatmapActive;
    const btn = document.getElementById('btn-toggle-heatmap');
    if (isHeatmapActive) {
        btn.classList.add('active', 'btn-warning');
        btn.classList.remove('btn-outline-warning');
    } else {
        btn.classList.remove('active', 'btn-warning');
        btn.classList.add('btn-outline-warning');
    }
    updateControls();
}

function toggleZoomLoupe() {
    isZoomLoupeActive = !isZoomLoupeActive;
    const row = document.getElementById('zoom-loupe-row');
    const btn = document.getElementById('btn-toggle-zoom');
    if (isZoomLoupeActive) {
        row.style.display = 'flex';
        btn.classList.add('active');
    } else {
        row.style.display = 'none';
        btn.classList.remove('active');
    }
}

function toggleNativeView() {
    isNativeViewActive = !isNativeViewActive;
    const rawImg = document.getElementById('raw-video-img');
    const upImg = document.getElementById('upscaled-video-img');
    const btn = document.getElementById('btn-toggle-native');

    if (isNativeViewActive) {
        btn.classList.add('active', 'btn-light');
        btn.classList.remove('btn-outline-light');
        rawImg.classList.remove('w-100');
        upImg.classList.remove('w-100');
        rawImg.style.maxHeight = 'none';
        upImg.style.maxHeight = 'none';
        rawImg.style.width = '640px';
        rawImg.style.height = '360px';
        upImg.style.width = '1280px';
        upImg.style.height = '720px';
    } else {
        btn.classList.remove('active', 'btn-light');
        btn.classList.add('btn-outline-light');
        rawImg.classList.add('w-100');
        upImg.classList.add('w-100');
        rawImg.style.maxHeight = '480px';
        upImg.style.maxHeight = '480px';
        rawImg.style.width = 'auto';
        rawImg.style.height = 'auto';
        upImg.style.width = 'auto';
        upImg.style.height = 'auto';
    }
}

function setupZoomListeners() {
    const rawImg = document.getElementById('raw-video-img');
    const upImg = document.getElementById('upscaled-video-img');

    function handleMouseMove(e, targetImg) {
        if (!targetImg || !targetImg.naturalWidth) return;
        const rect = targetImg.getBoundingClientRect();
        hoverNormX = Math.max(0.05, Math.min(0.95, (e.clientX - rect.left) / rect.width));
        hoverNormY = Math.max(0.05, Math.min(0.95, (e.clientY - rect.top) / rect.height));
        renderZoomLoupe();
    }

    if (rawImg) {
        rawImg.addEventListener('mousemove', (e) => handleMouseMove(e, rawImg));
    }
    if (upImg) {
        upImg.addEventListener('mousemove', (e) => handleMouseMove(e, upImg));
    }

    // Periodic zoom loupe refresher for live video feed
    setInterval(() => {
        if (cameraActive && isZoomLoupeActive) {
            renderZoomLoupe();
        }
    }, 100);
}

function renderZoomLoupe() {
    const rawImg = document.getElementById('raw-video-img');
    const upImg = document.getElementById('upscaled-video-img');
    const rawCanvas = document.getElementById('raw-zoom-canvas');
    const upCanvas = document.getElementById('upscaled-zoom-canvas');

    if (!rawCanvas || !upCanvas) return;
    const ctxRaw = rawCanvas.getContext('2d');
    const ctxUp = upCanvas.getContext('2d');

    if (rawImg && rawImg.complete && rawImg.naturalWidth > 0) {
        ctxRaw.imageSmoothingEnabled = false; // Show raw square pixels
        const cropW = 70;
        const cropH = 35;
        const sx = Math.max(0, Math.min(rawImg.naturalWidth - cropW, hoverNormX * rawImg.naturalWidth - cropW / 2));
        const sy = Math.max(0, Math.min(rawImg.naturalWidth - cropH, hoverNormY * rawImg.naturalHeight - cropH / 2));
        ctxRaw.clearRect(0, 0, rawCanvas.width, rawCanvas.height);
        ctxRaw.drawImage(rawImg, sx, sy, cropW, cropH, 0, 0, rawCanvas.width, rawCanvas.height);

        // Draw crosshair on zoom canvas
        ctxRaw.strokeStyle = 'rgba(0, 240, 255, 0.4)';
        ctxRaw.lineWidth = 1;
        ctxRaw.beginPath();
        ctxRaw.moveTo(rawCanvas.width / 2, 0); ctxRaw.lineTo(rawCanvas.width / 2, rawCanvas.height);
        ctxRaw.moveTo(0, rawCanvas.height / 2); ctxRaw.lineTo(rawCanvas.width, rawCanvas.height / 2);
        ctxRaw.stroke();
    }

    if (upImg && upImg.complete && upImg.naturalWidth > 0) {
        ctxUp.imageSmoothingEnabled = true; // Show sub-pixel interpolation
        const cropW = 70 * (upImg.naturalWidth / Math.max(1, rawImg.naturalWidth || 640));
        const cropH = 35 * (upImg.naturalHeight / Math.max(1, rawImg.naturalHeight || 360));
        const sx = Math.max(0, Math.min(upImg.naturalWidth - cropW, hoverNormX * upImg.naturalWidth - cropW / 2));
        const sy = Math.max(0, Math.min(upImg.naturalHeight - cropH, hoverNormY * upImg.naturalHeight - cropH / 2));
        ctxUp.clearRect(0, 0, upCanvas.width, upCanvas.height);
        ctxUp.drawImage(upImg, sx, sy, cropW, cropH, 0, 0, upCanvas.width, upCanvas.height);

        // Draw crosshair on zoom canvas
        ctxUp.strokeStyle = 'rgba(0, 240, 255, 0.4)';
        ctxUp.lineWidth = 1;
        ctxUp.beginPath();
        ctxUp.moveTo(upCanvas.width / 2, 0); ctxUp.lineTo(upCanvas.width / 2, upCanvas.height);
        ctxUp.moveTo(0, upCanvas.height / 2); ctxUp.lineTo(upCanvas.width, upCanvas.height / 2);
        ctxUp.stroke();
    }
}

function setWorkload(level) {
    ['normal', 'increased', 'high'].forEach(l => {
        const btn = document.getElementById('wl-' + l);
        if (btn) {
            btn.classList.remove('active', 'btn-secondary', 'btn-warning', 'btn-danger');
            if (l === 'normal') btn.classList.add('btn-outline-secondary');
            if (l === 'increased') btn.classList.add('btn-outline-warning');
            if (l === 'high') btn.classList.add('btn-outline-danger');
        }
    });

    const activeBtn = document.getElementById('wl-' + level.toLowerCase());
    if (activeBtn) {
        activeBtn.classList.add('active');
        if (level === 'NORMAL') activeBtn.classList.add('btn-secondary');
        if (level === 'INCREASED') activeBtn.classList.add('btn-warning', 'text-dark');
        if (level === 'HIGH') activeBtn.classList.add('btn-danger');
    }

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ workload: level })
    }).catch(err => console.error(err));
}

function pollMetrics() {
    if (!cameraActive) return;

    fetch('/api/status')
        .then(res => res.json())
        .then(data => {
            document.getElementById('stat-fps').innerText = data.current_fps ? data.current_fps.toFixed(1) : '--';
            document.getElementById('stat-target-fps').innerText = 'Target: ' + data.target_fps + ' FPS';
            document.getElementById('stat-interp-latency').innerText = data.interpolation_latency_ms + ' ms';
            document.getElementById('stat-budget').innerText = 'Budget: ' + data.target_budget_ms + ' ms';
            document.getElementById('stat-e2e-latency').innerText = data.end_to_end_latency_ms + ' ms';
            document.getElementById('stat-method').innerText = data.current_method;
            document.getElementById('stat-threads').innerText = data.num_threads + ' Threads';
            document.getElementById('stat-dropped').innerText = data.discarded_frames;
            document.getElementById('stat-switches').innerText = data.algorithm_switches + ' switches';

            const badge = document.getElementById('stat-adaptive-badge');
            badge.innerText = data.state_message;
            if (data.is_overloaded) {
                badge.className = 'badge bg-danger fs-6 pulse-overload';
            } else if (data.state_message.indexOf('Headroom') !== -1) {
                badge.className = 'badge bg-info text-dark fs-6';
            } else {
                badge.className = 'badge bg-success fs-6';
            }

            // Quality & Resolution Analytics
            if (data.raw_resolution) {
                document.getElementById('raw-res-pill').innerText = data.raw_resolution;
                document.getElementById('stat-raw-px').innerText = data.raw_resolution + ' (' + (data.raw_pixels || 230400).toLocaleString() + ' px)';
            }
            if (data.upscaled_resolution) {
                const scale = data.scale_factor || 2.0;
                document.getElementById('raw-res-pill').innerText = data.raw_resolution + ' (' + (data.raw_pixels || 230400).toLocaleString() + ' px)';
                document.getElementById('upscaled-res-pill').innerText = data.upscaled_resolution + ' (' + (data.upscaled_pixels || 921600).toLocaleString() + ' px | ' + (data.pixel_gain_pct || '+300%') + ')';
                document.getElementById('stat-up-px').innerText = data.upscaled_resolution + ' (' + (data.upscaled_pixels || 921600).toLocaleString() + ' px)';
                document.getElementById('stat-px-gain').innerText = data.pixel_gain_pct || '+300% Data';
            }

            if (data.raw_sharpness && data.upscaled_sharpness) {
                document.getElementById('stat-edge-gain').innerText = data.raw_sharpness + ' -> ' + data.upscaled_sharpness + ' (' + data.sharpness_gain_pct + ')';
                document.getElementById('raw-sharpness-badge').innerText = 'Sharpness: ' + data.raw_sharpness;
                document.getElementById('upscaled-sharpness-badge').innerText = 'Sharpness: ' + data.upscaled_sharpness + ' (' + data.sharpness_gain_pct + ')';
            }

            const tierEl = document.getElementById('stat-quality-tier');
            if (data.current_method === 'BICUBIC') {
                tierEl.innerText = 'BICUBIC (Catmull-Rom Keys Spline - High Fidelity)';
                tierEl.className = 'text-success';
            } else if (data.current_method === 'BILINEAR') {
                tierEl.innerText = 'BILINEAR (Continuous Linear Gradient - Medium)';
                tierEl.className = 'text-info';
            } else {
                tierEl.innerText = 'NEAREST (1-Neighbor Grid Replication - Baseline)';
                tierEl.className = 'text-warning';
            }

            if (typeof updateChartData === 'function') {
                updateChartData(data.interpolation_latency_ms, data.target_budget_ms, data.current_fps);
            }

            if (data.decisions && data.decisions.length > 0) {
                document.getElementById('decision-count-badge').innerText = data.decisions.length + ' Events';
                const tbody = document.getElementById('decision-log-tbody');
                tbody.innerHTML = '';
                data.decisions.slice().reverse().forEach(d => {
                    const row = document.createElement('tr');
                    const isDowngrade = d.reason.toLowerCase().includes('overload') || d.reason.toLowerCase().includes('downgrading');
                    const colorClass = isDowngrade ? 'text-danger fw-bold' : 'text-success fw-bold';
                    
                    row.innerHTML = 
                        '<td>' + d.timestamp + '</td>' +
                        '<td>#' + d.frame + '</td>' +
                        '<td><span class="' + colorClass + '">' + d.from_method + ' &rarr; ' + d.to_method + '</span></td>' +
                        '<td>' + d.latency_ms + ' / ' + d.budget_ms + ' ms</td>' +
                        '<td class="text-truncate" style="max-width: 200px;" title="' + d.reason + '">' + d.reason + '</td>';
                    tbody.appendChild(row);
                });
            }
        })
        .catch(err => console.error('Metrics error:', err));
}

// UPLOADED VIDEO LOGIC
let videoPollInterval = null;
let videoFramePollInterval = null;
let totalPreviewFrames = 0;
let currentPreviewFrame = 0;
let isPlayerPlaying = false;
let playerPlaybackTimer = null;

function useDemoVideo() {
    startVideoJob({ use_sample: true });
}

function uploadAndProcessVideo(e) {
    if (e && e.preventDefault) e.preventDefault();
    const fileInput = document.getElementById('video-file-input');
    if (!fileInput || !fileInput.files.length) {
        alert('Please choose a video file or click "Quick Test with Built-in Demo Video".');
        return;
    }
    startVideoJob({ file: fileInput.files[0] });
}

function startVideoJob(options) {
    const formData = new FormData();
    if (options.use_sample) {
        formData.append('use_sample', 'true');
    } else {
        formData.append('video', options.file);
    }

    formData.append('scale', document.getElementById('vid-scale').value);
    formData.append('policy', document.getElementById('vid-policy').value);
    formData.append('method', document.getElementById('vid-method').value);
    formData.append('threads', document.getElementById('vid-threads').value);
    formData.append('adaptive', document.getElementById('vid-adaptive').checked);

    const submitBtn = document.getElementById('vid-submit-btn');
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Processing...';

    document.getElementById('vid-progress-box').style.display = 'block';
    document.getElementById('vid-placeholder').style.display = 'none';
    document.getElementById('vid-player-box').style.display = 'none';
    document.getElementById('vid-summary-card').style.display = 'none';
    document.getElementById('vid-download-btn').style.display = 'none';
    document.getElementById('vid-live-preview-box').style.display = 'block';

    fetch('/api/upload_video', {
        method: 'POST',
        body: formData
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === 'started') {
            if (videoPollInterval) clearInterval(videoPollInterval);
            if (videoFramePollInterval) clearInterval(videoFramePollInterval);

            videoPollInterval = setInterval(pollVideoProgress, 350);
            videoFramePollInterval = setInterval(pollLiveVideoFrame, 150);
        } else {
            alert('Error: ' + data.message);
            submitBtn.disabled = false;
            submitBtn.innerHTML = '<i class="bi bi-play-circle me-1"></i> Start Processing';
        }
    })
    .catch(err => {
        alert('Upload failed: ' + err);
        submitBtn.disabled = false;
        submitBtn.innerHTML = '<i class="bi bi-play-circle me-1"></i> Start Processing';
    });
}

function pollLiveVideoFrame() {
    const liveImg = document.getElementById('vid-live-frame-img');
    liveImg.src = '/api/video_current_frame?t=' + Date.now();
}

function pollVideoProgress() {
    fetch('/api/video_progress')
        .then(res => res.json())
        .then(data => {
            if (data.error) {
                alert('Processing error: ' + data.error);
                finishVideoProcessing();
                return;
            }

            const bar = document.getElementById('vid-progress-bar');
            const pct = document.getElementById('vid-prog-pct');
            const statFrame = document.getElementById('vid-stat-frame');
            const statFps = document.getElementById('vid-stat-fps');

            bar.style.width = data.progress_pct + '%';
            pct.innerText = data.progress_pct + '%';
            statFrame.innerText = 'Frame: ' + data.current_frame + ' / ' + data.total_frames;
            statFps.innerText = 'Speed: ' + data.current_fps + ' FPS (' + data.current_method + ')';
            document.getElementById('vid-live-res-badge').innerText = (data.current_fps || 0) + ' FPS | ' + data.current_method;

            if (data.is_complete) {
                finishVideoProcessing(data);
            }
        })
        .catch(err => console.error(err));
}

function finishVideoProcessing(data) {
    if (videoPollInterval) clearInterval(videoPollInterval);
    if (videoFramePollInterval) clearInterval(videoFramePollInterval);
    videoPollInterval = null;
    videoFramePollInterval = null;

    const submitBtn = document.getElementById('vid-submit-btn');
    submitBtn.disabled = false;
    submitBtn.innerHTML = '<i class="bi bi-play-circle me-1"></i> Start Processing';

    if (!data || data.error) return;

    document.getElementById('vid-live-preview-box').style.display = 'none';
    document.getElementById('vid-player-box').style.display = 'block';

    totalPreviewFrames = data.preview_frame_count || 1;
    currentPreviewFrame = 0;

    const slider = document.getElementById('vid-frame-slider');
    slider.max = Math.max(0, totalPreviewFrames - 1);
    slider.value = 0;
    document.getElementById('vid-player-counter').innerText = 'Frame 1 / ' + totalPreviewFrames;
    document.getElementById('vid-final-res-badge').innerText = (data.dst_resolution || 'Upscaled') + ' (' + totalPreviewFrames + ' frames)';

    loadPlayerFrame(0);

    const downloadBtn = document.getElementById('vid-download-btn');
    downloadBtn.style.display = 'inline-block';
    downloadBtn.href = '/api/download_video';

    const summaryCard = document.getElementById('vid-summary-card');
    summaryCard.style.display = 'block';
    const srcParts = (data.src_resolution || '640x360').split('x');
    const dstParts = (data.dst_resolution || '1280x720').split('x');
    const srcPx = parseInt(srcParts[0]) * parseInt(srcParts[1]);
    const dstPx = parseInt(dstParts[0]) * parseInt(dstParts[1]);
    const pxGain = Math.round(((dstPx - srcPx) / srcPx) * 100);
    document.getElementById('sum-res').innerText = data.src_resolution + ' (' + srcPx.toLocaleString() + ' px) -> ' + data.dst_resolution + ' (' + dstPx.toLocaleString() + ' px | +' + pxGain + '%)';
    document.getElementById('sum-time').innerText = data.total_time_sec + 's';
    document.getElementById('sum-lat').innerText = data.average_latency_ms + ' ms';
    document.getElementById('sum-fps').innerText = data.achieved_fps + ' FPS';

    startPlayerPlayback();
}

function loadPlayerFrame(idx) {
    currentPreviewFrame = idx;
    document.getElementById('vid-player-img').src = '/api/video_preview_frame/' + idx;
    document.getElementById('vid-frame-slider').value = idx;
    document.getElementById('vid-player-counter').innerText = 'Frame ' + (idx + 1) + ' / ' + totalPreviewFrames;
}

function onPlayerSliderChange(val) {
    if (isPlayerPlaying) stopPlayerPlayback();
    loadPlayerFrame(parseInt(val));
}

function togglePlayerPlayback() {
    if (isPlayerPlaying) {
        stopPlayerPlayback();
    } else {
        startPlayerPlayback();
    }
}

function startPlayerPlayback() {
    if (totalPreviewFrames <= 1) return;
    isPlayerPlaying = true;
    document.getElementById('play-btn-icon').className = 'bi bi-pause-fill';

    if (playerPlaybackTimer) clearInterval(playerPlaybackTimer);
    playerPlaybackTimer = setInterval(() => {
        let nextFrame = currentPreviewFrame + 1;
        if (nextFrame >= totalPreviewFrames) {
            nextFrame = 0;
        }
        loadPlayerFrame(nextFrame);
    }, 40);
}

function stopPlayerPlayback() {
    isPlayerPlaying = false;
    document.getElementById('play-btn-icon').className = 'bi bi-play-fill';
    if (playerPlaybackTimer) {
        clearInterval(playerPlaybackTimer);
        playerPlaybackTimer = null;
    }
}


// PHOTO / INTERVIEW SNAPSHOT UPSCALE LOGIC
function useDemoPhoto() {
    startPhotoUpscale({ use_sample: true });
}

function captureWebcamSnapshot() {
    startPhotoUpscale({ from_webcam: true });
}

function uploadAndProcessPhoto(e) {
    if (e && e.preventDefault) e.preventDefault();
    const fileInput = document.getElementById('photo-file-input');
    if (!fileInput || !fileInput.files.length) {
        alert('Please choose a photo file, capture from webcam, or click "Interview Snapshot Demo".');
        return;
    }
    startPhotoUpscale({ file: fileInput.files[0] });
}

function startPhotoUpscale(options) {
    const formData = new FormData();
    if (options.use_sample) {
        formData.append('use_sample', 'true');
    } else if (options.from_webcam) {
        formData.append('from_webcam', 'true');
    } else {
        formData.append('photo', options.file);
    }

    const scale = document.getElementById('photo-scale').value;
    const policy = document.getElementById('photo-policy').value;
    const method = document.getElementById('photo-method').value;
    const threads = document.getElementById('photo-threads').value;

    formData.append('scale', scale);
    formData.append('policy', policy);
    formData.append('method', method);
    formData.append('threads', threads);

    const submitBtn = document.getElementById('photo-submit-btn');
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Upscaling with C++ OpenMP...';

    fetch('/api/upscale_photo', {
        method: 'POST',
        body: formData
    })
    .then(res => res.json())
    .then(data => {
        submitBtn.disabled = false;
        submitBtn.innerHTML = '<i class="bi bi-stars me-1"></i> Upscale Photo with OpenMP C++';

        if (data.status === 'ok') {
            document.getElementById('photo-placeholder').style.display = 'none';
            document.getElementById('photo-result-box').style.display = 'block';

            // Exact Pixel Count Before & After
            document.getElementById('photo-before-res').innerText = data.src_resolution;
            document.getElementById('photo-before-px').innerText = Number(data.src_pixels).toLocaleString() + ' Pixels';
            document.getElementById('photo-after-res').innerText = data.dst_resolution;
            document.getElementById('photo-after-px').innerText = Number(data.dst_pixels).toLocaleString() + ' Pixels (' + data.pixel_gain_pct + ' Data)';
            document.getElementById('photo-scale-badge').innerText = scale + 'x Upscale';

            // Images and pills
            document.getElementById('photo-src-img').src = data.src_image_url;
            document.getElementById('photo-dst-img').src = data.dst_image_url;
            document.getElementById('photo-src-pill').innerText = data.src_resolution + ' (' + Number(data.src_pixels).toLocaleString() + ' px)';
            document.getElementById('photo-dst-pill').innerText = data.dst_resolution + ' (' + Number(data.dst_pixels).toLocaleString() + ' px - ' + data.pixel_gain_pct + ')';

            // Performance & Sharpness metrics
            document.getElementById('photo-stat-lat').innerText = data.latency_ms + ' ms';
            document.getElementById('photo-stat-method').innerText = data.method;
            document.getElementById('photo-stat-threads').innerText = data.threads + ' Threads (' + data.policy + ')';
            document.getElementById('photo-stat-sharpness').innerText = data.src_sharpness + ' -> ' + data.dst_sharpness + ' (' + data.sharpness_gain_pct + ')';

            // Download Button
            const dlBtn = document.getElementById('photo-download-btn');
            dlBtn.style.display = 'inline-block';
            dlBtn.href = data.download_url;
            dlBtn.download = data.filename || 'upscaled_photo.jpg';
        } else {
            alert('Upscale failed: ' + (data.message || 'Unknown error'));
        }
    })
    .catch(err => {
        submitBtn.disabled = false;
        submitBtn.innerHTML = '<i class="bi bi-stars me-1"></i> Upscale Photo with OpenMP C++';
        alert('Network error during photo upscale: ' + err);
        console.error(err);
    });
}
