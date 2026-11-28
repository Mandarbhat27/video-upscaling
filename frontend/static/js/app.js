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

let cctvActive = false;

function switchMode(mode) {
    currentMode = mode;
    const webcamSec = document.getElementById('webcam-section');
    const cctvSec = document.getElementById('cctv-section');
    const videoSec = document.getElementById('video-section');
    const photoSec = document.getElementById('photo-section');

    const webcamBtn = document.getElementById('mode-webcam-btn');
    const cctvBtn = document.getElementById('mode-cctv-btn');
    const videoBtn = document.getElementById('mode-video-btn');
    const photoBtn = document.getElementById('mode-photo-btn');

    [webcamSec, cctvSec, videoSec, photoSec].forEach(sec => { if (sec) sec.style.display = 'none'; });
    [webcamBtn, cctvBtn, videoBtn, photoBtn].forEach(btn => {
        if (btn) {
            btn.classList.remove('btn-primary', 'btn-warning', 'btn-info', 'btn-success', 'active');
            btn.classList.add('btn-outline-secondary');
        }
    });

    if (mode === 'webcam') {
        if (webcamSec) webcamSec.style.display = 'block';
        if (webcamBtn) { webcamBtn.classList.add('btn-primary', 'active'); webcamBtn.classList.remove('btn-outline-secondary'); }
    } else if (mode === 'cctv') {
        if (cctvSec) cctvSec.style.display = 'block';
        if (cctvBtn) { cctvBtn.classList.add('btn-warning', 'text-dark', 'active'); cctvBtn.classList.remove('btn-outline-secondary'); }
    } else if (mode === 'video') {
        if (videoSec) videoSec.style.display = 'block';
        if (videoBtn) { videoBtn.classList.add('btn-info', 'text-dark', 'active'); videoBtn.classList.remove('btn-outline-secondary'); }
    } else if (mode === 'photo') {
        if (photoSec) photoSec.style.display = 'block';
        if (photoBtn) { photoBtn.classList.add('btn-success', 'text-light', 'active'); photoBtn.classList.remove('btn-outline-secondary'); }
    }
}

function updateCCTVUrlFromCredentials() {
    const ip = (document.getElementById('cctv-ip') ? document.getElementById('cctv-ip').value : '').trim();
    const port = (document.getElementById('cctv-port') ? document.getElementById('cctv-port').value : '554').trim();
    const user = (document.getElementById('cctv-user') ? document.getElementById('cctv-user').value : '').trim();
    const pass = (document.getElementById('cctv-pass') ? document.getElementById('cctv-pass').value : '').trim();
    const path = (document.getElementById('cctv-path') ? document.getElementById('cctv-path').value : '').trim();

    if (!ip) return;
    let url = '';
    if (port === '8080') {
        url = `http://${user && pass ? user + ':' + pass + '@' : ''}${ip}:${port}${path.startsWith('/') ? path : '/' + path}`;
    } else {
        url = `rtsp://${user && pass ? user + ':' + pass + '@' : ''}${ip}:${port}${path.startsWith('/') ? path : '/' + path}`;
    }
    const urlInput = document.getElementById('cctv-stream-url');
    if (urlInput) urlInput.value = url;
}

function setCCTVPreset(preset) {
    const ipInput = document.getElementById('cctv-ip');
    const portInput = document.getElementById('cctv-port');
    const userInput = document.getElementById('cctv-user');
    const passInput = document.getElementById('cctv-pass');
    const pathInput = document.getElementById('cctv-path');
    const urlInput = document.getElementById('cctv-stream-url');

    if (preset === 'phone') {
        if (ipInput) ipInput.value = '192.168.1.38';
        if (portInput) portInput.value = '8080';
        if (userInput) userInput.value = '';
        if (passInput) passInput.value = '';
        if (pathInput) pathInput.value = '/video';
        if (urlInput) urlInput.value = 'http://192.168.1.38:8080/video';
    } else if (preset === 'rtsp') {
        if (ipInput) ipInput.value = '192.168.1.64';
        if (portInput) portInput.value = '554';
        if (userInput) userInput.value = 'admin';
        if (passInput) passInput.value = 'admin123';
        if (pathInput) pathInput.value = '/h264/ch1/main';
        if (urlInput) urlInput.value = 'rtsp://admin:admin123@192.168.1.64:554/h264/ch1/main';
    } else if (preset === 'demo') {
        if (ipInput) ipInput.value = 'Campus Gate 1 (Demo)';
        if (portInput) portInput.value = '554';
        if (userInput) userInput.value = 'admin';
        if (passInput) passInput.value = '••••••';
        if (pathInput) pathInput.value = '/live/gate1';
        if (urlInput) urlInput.value = ''; // Empty string signals campus gate demo feed in backend
    }
}

function toggleWebcam() {
    toggleCamera('webcam');
}

function toggleCCTVStream() {
    const streamUrl = document.getElementById('cctv-stream-url') ? document.getElementById('cctv-stream-url').value.trim() : '';
    toggleCamera('cctv', streamUrl);
}

let currentStreamMode = 'webcam';

function onStreamSourceChange(mode) {
    currentStreamMode = mode;
    const cctvBox = document.getElementById('cctv-config-box');
    const badge = document.getElementById('active-stream-label');

    if (mode === 'cctv_url') {
        if (cctvBox) cctvBox.style.display = 'block';
        if (badge) badge.innerText = 'Target: Live IP / CCTV Camera';
    } else if (mode === 'cctv_campus') {
        if (cctvBox) cctvBox.style.display = 'none';
        if (badge) badge.innerText = 'Campus Gate 1 CCTV [Live Demo]';
    } else {
        if (cctvBox) cctvBox.style.display = 'none';
        if (badge) badge.innerText = 'Local USB Webcam';
    }

    // Auto-restart if already streaming
    if (cameraActive) {
        toggleCamera();
        setTimeout(() => toggleCamera(), 400);
    }
}

function setCCTVPreset(preset) {
    const urlInput = document.getElementById('cctv-stream-url');
    if (urlInput) {
        if (preset === 'phone') {
            urlInput.value = 'http://192.168.1.15:8080/video';
            urlInput.focus();
        }
    }
}

function toggleCredentialsHelper() {
    const el = document.getElementById('cctv-cred-helper');
    if (el) {
        el.style.display = (el.style.display === 'none') ? 'block' : 'none';
    }
}

function applyRTSPBuilder() {
    const ip = (document.getElementById('rtsp-ip').value || '').trim();
    const port = (document.getElementById('rtsp-port').value || '').trim() || '554';
    const user = (document.getElementById('rtsp-user').value || '').trim();
    const pass = (document.getElementById('rtsp-pass').value || '').trim();

    if (!ip) {
        alert('Please enter camera IP address');
        return;
    }

    let url = 'rtsp://';
    if (user && pass) {
        url += `${encodeURIComponent(user)}:${encodeURIComponent(pass)}@`;
    } else if (user) {
        url += `${encodeURIComponent(user)}@`;
    }
    url += `${ip}:${port}/Streaming/Channels/101`;
    document.getElementById('cctv-stream-url').value = url;
    document.getElementById('cctv-cred-helper').style.display = 'none';
}

function toggleCamera(mode = 'webcam', streamUrl = '') {
    const isCCTV = mode === 'cctv' || currentMode === 'cctv';
    const btn = isCCTV ? document.getElementById('cctv-toggle-btn') : document.getElementById('cam-toggle-btn');
    const rawImg = isCCTV ? document.getElementById('cctv-raw-video-img') : document.getElementById('raw-video-img');
    const upscaledImg = isCCTV ? document.getElementById('cctv-upscaled-video-img') : document.getElementById('upscaled-video-img');
    const rawPlaceholder = isCCTV ? document.getElementById('cctv-raw-placeholder') : document.getElementById('raw-placeholder');
    const upscaledPlaceholder = isCCTV ? document.getElementById('cctv-upscaled-placeholder') : document.getElementById('upscaled-placeholder');
    const recPill = isCCTV ? document.getElementById('cctv-rec-pill') : document.getElementById('webcam-rec-pill');
    const downloadBtn = isCCTV ? document.getElementById('cctv-download-btn') : document.getElementById('webcam-download-btn');
    const framesBadge = isCCTV ? document.getElementById('cctv-rec-frames-badge') : document.getElementById('webcam-rec-frames-badge');

    if (!cameraActive) {
        if (btn) {
            btn.disabled = true;
            btn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Connecting...';
        }

        if (downloadBtn) downloadBtn.style.display = 'none';

        const finalUrl = streamUrl || (document.getElementById('cctv-stream-url') ? document.getElementById('cctv-stream-url').value.trim() : '');

        fetch('/api/camera/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                mode: isCCTV ? 'cctv' : 'webcam',
                stream_url: isCCTV ? finalUrl : ''
            })
        })
            .then(res => res.json())
            .then(data => {
                if (btn) btn.disabled = false;
                if (data.status === 'ok') {
                    cameraActive = true;
                    if (btn) {
                        btn.classList.remove('btn-success', 'btn-warning');
                        btn.classList.add('btn-danger');
                        btn.innerHTML = '<i class="bi bi-stop-fill me-1"></i> Stop & Save Stream';
                    }

                    if (recPill) recPill.style.display = 'inline-block';

                    if (rawPlaceholder) rawPlaceholder.style.display = 'none';
                    if (upscaledPlaceholder) upscaledPlaceholder.style.display = 'none';
                    if (rawImg) rawImg.style.display = 'block';
                    if (upscaledImg) upscaledImg.style.display = 'block';

                    const ts = Date.now();
                    if (rawImg) rawImg.src = '/video_feed/raw?ts=' + ts;
                    if (upscaledImg) upscaledImg.src = '/video_feed/upscaled?ts=' + ts;

                    if (isCCTV) {
                        const lbl = document.getElementById('cctv-active-label');
                        if (lbl) lbl.innerText = data.source_label || 'Live CCTV Connected';
                    }

                    if (!metricsInterval) {
                        metricsInterval = setInterval(pollMetrics, 300);
                    }
                } else {
                    alert('⚠️ Live Camera Connection Failed:\n' + (data.message || 'Could not connect to camera') + '\n\nSteps to check on your phone:\n1. Open IP Webcam app and tap \'Start server\' at the bottom.\n2. Make sure your phone and laptop are connected to the SAME Wi-Fi network.\n3. Check that the URL is: http://192.168.1.38:8080/video');
                    if (btn) {
                        btn.classList.remove('btn-danger');
                        btn.classList.add(isCCTV ? 'btn-warning' : 'btn-success');
                        btn.innerHTML = isCCTV ? '<i class="bi bi-play-circle-fill me-1"></i> Connect & Enhance CCTV Live' : '<i class="bi bi-play-fill me-1"></i> Start Live Webcam';
                    }
                }
            })
            .catch(err => {
                if (btn) {
                    btn.disabled = false;
                    btn.classList.remove('btn-danger');
                    btn.classList.add(isCCTV ? 'btn-warning' : 'btn-success');
                    btn.innerHTML = isCCTV ? '<i class="bi bi-play-circle-fill me-1"></i> Connect & Enhance CCTV Live' : '<i class="bi bi-play-fill me-1"></i> Start Live Webcam';
                }
                console.error(err);
            });
    } else {
        if (btn) {
            btn.disabled = true;
            btn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Finalizing Recording...';
        }

        fetch('/api/camera/stop', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (btn) btn.disabled = false;
                cameraActive = false;
                if (btn) {
                    btn.classList.remove('btn-danger');
                    btn.classList.add(isCCTV ? 'btn-warning' : 'btn-success');
                    btn.innerHTML = isCCTV ? '<i class="bi bi-play-circle-fill me-1"></i> Connect & Enhance CCTV Live' : '<i class="bi bi-play-fill me-1"></i> Start Live Webcam';
                }

                if (recPill) recPill.style.display = 'none';

                if (rawImg) { rawImg.style.display = 'none'; rawImg.src = ''; }
                if (upscaledImg) { upscaledImg.style.display = 'none'; upscaledImg.src = ''; }
                if (rawPlaceholder) rawPlaceholder.style.display = 'block';
                if (upscaledPlaceholder) upscaledPlaceholder.style.display = 'block';

                // Display recorded live video download button immediately
                if (data.has_recorded_video && downloadBtn) {
                    downloadBtn.style.display = 'inline-flex';
                    if (framesBadge) {
                        framesBadge.innerText = `${data.recorded_frames || 'Full'} Frames Saved`;
                    }
                }

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
                if (btn) btn.disabled = false;
                console.error(err);
            });
    }
}

function startCameraPollingFallback() {
    if (cameraPollInterval) return;
    const rawImg = document.getElementById('raw-video-img');
    const upscaledImg = document.getElementById('upscaled-video-img');

    const targetFpsVal = parseFloat(document.getElementById('ctrl-target-fps') ? document.getElementById('ctrl-target-fps').value : 120);
    const pollIntervalMs = targetFpsVal >= 120 ? 8 : (targetFpsVal >= 60 ? 16 : 33);
    cameraPollInterval = setInterval(() => {
        if (!cameraActive) return;
        const ts = Date.now();
        rawImg.src = '/api/camera/frame/raw?ts=' + ts;
        upscaledImg.src = '/api/camera/frame/upscaled?ts=' + ts;
    }, pollIntervalMs);
}

function updateControls() {
    const scale = document.getElementById('ctrl-scale').value;
    const policy = document.getElementById('ctrl-policy').value;
    const threads = document.getElementById('ctrl-threads').value;
    const targetFps = document.getElementById('ctrl-target-fps').value;
    const adaptive = document.getElementById('ctrl-adaptive').checked;
    const enhEl = document.getElementById('ctrl-enhancement');
    const enhancement = enhEl ? parseFloat(enhEl.value) : 3.5;

    const bEl = document.getElementById('ctrl-brightness');
    const cEl = document.getElementById('ctrl-contrast');
    const llEl = document.getElementById('ctrl-lowlight');
    const dnEl = document.getElementById('ctrl-denoise');

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            scale: scale,
            policy: policy,
            threads: threads,
            target_fps: targetFps,
            adaptive: adaptive,
            enhancement: enhancement,
            show_heatmap: isHeatmapActive,
            brightness: bEl ? parseInt(bEl.value) : 0,
            contrast: cEl ? parseFloat(cEl.value) : 1.0,
            low_light_boost: llEl ? llEl.checked : false,
            denoise: dnEl ? dnEl.checked : false
        })
    }).catch(err => console.error(err));
}

function updateLightingControls() {
    const bEl = document.getElementById('ctrl-brightness');
    const cEl = document.getElementById('ctrl-contrast');
    const llEl = document.getElementById('ctrl-lowlight');
    const dnEl = document.getElementById('ctrl-denoise');

    const brightness = bEl ? parseInt(bEl.value) : 0;
    const contrast = cEl ? parseFloat(cEl.value) : 1.0;
    const lowlight = llEl ? llEl.checked : false;
    const denoise = dnEl ? dnEl.checked : false;

    const bValEl = document.getElementById('val-brightness');
    if (bValEl) bValEl.innerText = (brightness > 0 ? '+' : '') + brightness;

    const cValEl = document.getElementById('val-contrast');
    if (cValEl) cValEl.innerText = contrast.toFixed(1) + 'x';

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            brightness: brightness,
            contrast: contrast,
            low_light_boost: lowlight,
            denoise: denoise
        })
    }).catch(err => console.error(err));
}

function resetLightingControls() {
    if (document.getElementById('ctrl-brightness')) document.getElementById('ctrl-brightness').value = 0;
    if (document.getElementById('ctrl-contrast')) document.getElementById('ctrl-contrast').value = 1.0;
    if (document.getElementById('ctrl-lowlight')) document.getElementById('ctrl-lowlight').checked = false;
    if (document.getElementById('ctrl-denoise')) document.getElementById('ctrl-denoise').checked = false;
    updateLightingControls();
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

            if (data.stream_source_label) {
                const srcLbl = document.getElementById('active-stream-label');
                if (srcLbl) srcLbl.innerText = data.stream_source_label;
            }
            // Update CCTV Mode Telemetry
            if (document.getElementById('cctv-stat-fps')) {
                document.getElementById('cctv-stat-fps').innerText = data.current_fps ? data.current_fps.toFixed(1) : '--';
            }
            if (document.getElementById('cctv-stat-latency')) {
                document.getElementById('cctv-stat-latency').innerText = data.interpolation_latency_ms + ' ms';
            }
            if (document.getElementById('cctv-raw-res-pill')) {
                document.getElementById('cctv-raw-res-pill').innerText = data.raw_res || '640x360';
            }
            if (document.getElementById('cctv-upscaled-res-pill')) {
                document.getElementById('cctv-upscaled-res-pill').innerText = (data.upscaled_res || '1280x720') + ' (' + data.scale_factor + 'x)';
            }
            if (document.getElementById('cctv-stat-gain')) {
                document.getElementById('cctv-stat-gain').innerText = data.sharpness_gain_pct || '+1,477%';
            }

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
    const vidEnh = document.getElementById('vid-enhancement');
    formData.append('enhancement', vidEnh ? vidEnh.value : '3.5');

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
    const splitSrc = document.getElementById('vid-split-src-res');
    const splitDst = document.getElementById('vid-split-dst-res');
    if (splitSrc) splitSrc.innerText = data.src_resolution || 'Original';
    if (splitDst) splitDst.innerText = data.dst_resolution || 'Upscaled';
    setVidSplitPosition(50);
    initVidSplitSliderOnce();

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
    const afterImg = document.getElementById('vid-player-after-img');
    const beforeImg = document.getElementById('vid-player-before-img');
    if (afterImg) afterImg.src = '/api/video_preview_frame/' + idx;
    if (beforeImg) beforeImg.src = '/api/video_raw_preview_frame/' + idx;
    
    // Fallback if single img exists
    const singleImg = document.getElementById('vid-player-img');
    if (singleImg) singleImg.src = '/api/video_preview_frame/' + idx;

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


// PHOTO / INTERVIEW SNAPSHOT UPSCALE & ENHANCEMENT LOGIC
let currentSrcPhotoUrl = null;
let currentDstPhotoUrl = null;
let photoLoupeActive = false;
let isSplitDragging = false;

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
    const enhEl = document.getElementById('photo-enhancement');
    const enhancement = enhEl ? enhEl.value : '3.5';

    formData.append('scale', scale);
    formData.append('policy', policy);
    formData.append('method', method);
    formData.append('threads', threads);
    formData.append('enhancement', enhancement);

    const submitBtn = document.getElementById('photo-submit-btn');
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Upscaling & Enhancing with C++ OpenMP...';

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

            currentSrcPhotoUrl = data.src_image_url;
            currentDstPhotoUrl = data.dst_image_url;

            // 1. Exact Pixel Count & Sharpness Banner
            document.getElementById('photo-before-res').innerText = data.src_resolution;
            document.getElementById('photo-before-px').innerText = Number(data.src_pixels).toLocaleString() + ' Pixels';
            document.getElementById('photo-src-sharpness-badge').innerText = 'Sharpness: ' + data.src_sharpness;

            document.getElementById('photo-after-res').innerText = data.dst_resolution;
            document.getElementById('photo-after-px').innerText = Number(data.dst_pixels).toLocaleString() + ' Pixels (' + data.pixel_gain_pct + ')';
            document.getElementById('photo-dst-sharpness-badge').innerText = 'Sharpness: ' + data.dst_sharpness + ' (' + data.sharpness_gain_pct + ')';
            document.getElementById('photo-scale-badge').innerText = scale + 'x Upscale';

            // 2. Setup Split Wipe Slider Images
            document.getElementById('split-after-img').src = data.dst_image_url;
            document.getElementById('split-before-img').src = data.src_image_url;
            document.getElementById('split-dst-res').innerText = data.dst_resolution;
            document.getElementById('split-src-res').innerText = data.src_resolution;
            
            // Reset split divider to center 50%
            setSplitPosition(50);

            // 3. Setup Side-by-Side Dual View Images
            document.getElementById('photo-src-img').src = data.src_image_url;
            document.getElementById('photo-dst-img').src = data.dst_image_url;
            document.getElementById('photo-src-pill').innerText = data.src_resolution + ' (' + Number(data.src_pixels).toLocaleString() + ' px)';
            document.getElementById('photo-dst-pill').innerText = data.dst_resolution + ' (' + Number(data.dst_pixels).toLocaleString() + ' px - ' + data.pixel_gain_pct + ')';

            // 4. Performance & OpenMP Telemetry
            document.getElementById('photo-stat-lat').innerText = data.latency_ms + ' ms';
            document.getElementById('photo-stat-method').innerText = data.method;
            document.getElementById('photo-stat-threads').innerText = data.threads + ' Threads (' + data.policy + ')';
            document.getElementById('photo-stat-enhancement').innerText = '+' + data.enhancement + 'x Edge Boost';

            // 5. Download Button
            const dlBtn = document.getElementById('photo-download-btn');
            dlBtn.style.display = 'inline-block';
            dlBtn.href = data.download_url;
            dlBtn.download = data.filename || 'upscaled_photo.jpg';

            // Ensure split slider handlers are initialized
            initSplitSliderOnce();

            if (photoLoupeActive) {
                const loupe = document.getElementById('photo-loupe-circle');
                if (loupe) loupe.style.backgroundImage = "url('" + currentDstPhotoUrl + "')";
            }
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

function switchPhotoView(mode) {
    const splitCont = document.getElementById('split-photo-container');
    const splitBar = document.getElementById('split-instructions-bar');
    const sideCont = document.getElementById('side-photo-container');
    const btnSplit = document.getElementById('btn-view-split');
    const btnSide = document.getElementById('btn-view-side');

    if (mode === 'split') {
        splitCont.style.display = 'block';
        if (splitBar) splitBar.style.display = 'flex';
        sideCont.style.display = 'none';
        btnSplit.classList.add('active', 'btn-info');
        btnSplit.classList.remove('btn-outline-info');
        btnSide.classList.remove('active', 'btn-secondary');
        btnSide.classList.add('btn-outline-secondary');
    } else {
        splitCont.style.display = 'none';
        if (splitBar) splitBar.style.display = 'none';
        sideCont.style.display = 'flex';
        btnSide.classList.add('active', 'btn-secondary');
        btnSide.classList.remove('btn-outline-secondary');
        btnSplit.classList.remove('active', 'btn-info');
        btnSplit.classList.add('btn-outline-info');
    }
}

function setSplitPosition(pct) {
    pct = Math.max(0, Math.min(100, pct));
    const beforeImg = document.getElementById('split-before-img');
    const divider = document.getElementById('split-divider-line');
    const label = document.getElementById('split-pos-label');

    if (beforeImg) {
        beforeImg.style.clipPath = 'polygon(0 0, ' + pct + '% 0, ' + pct + '% 100%, 0 100%)';
    }
    if (divider) {
        divider.style.left = pct + '%';
    }
    if (label) {
        label.innerText = 'Divider: ' + Math.round(pct) + '%';
    }
}

let splitSliderInitialized = false;
function initSplitSliderOnce() {
    if (splitSliderInitialized) return;
    const container = document.getElementById('split-photo-container');
    if (!container) return;
    splitSliderInitialized = true;

    function handleMove(clientX) {
        const rect = container.getBoundingClientRect();
        const x = clientX - rect.left;
        const pct = (x / rect.width) * 100;
        setSplitPosition(pct);
    }

    container.addEventListener('mousedown', (e) => {
        if (autoWipeActive) stopAutoWipe();
        isSplitDragging = true;
        handleMove(e.clientX);
    });

    window.addEventListener('mouseup', () => {
        isSplitDragging = false;
    });

    container.addEventListener('mousemove', (e) => {
        if (isSplitDragging) {
            handleMove(e.clientX);
        }
        if (photoLoupeActive) {
            updatePhotoLoupe(e);
        }
    });

    container.addEventListener('touchstart', (e) => {
        if (autoWipeActive) stopAutoWipe();
        if (e.touches.length > 0) {
            isSplitDragging = true;
            handleMove(e.touches[0].clientX);
        }
    }, { passive: true });

    container.addEventListener('touchmove', (e) => {
        if (isSplitDragging && e.touches.length > 0) {
            handleMove(e.touches[0].clientX);
        }
    }, { passive: true });

    window.addEventListener('touchend', () => {
        isSplitDragging = false;
    });
}

function togglePhotoLoupe() {
    photoLoupeActive = !photoLoupeActive;
    const loupe = document.getElementById('photo-loupe-circle');
    const btn = document.getElementById('btn-photo-loupe');
    const text = document.getElementById('loupe-btn-text');

    if (photoLoupeActive) {
        btn.classList.add('active', 'btn-warning');
        btn.classList.remove('btn-outline-warning');
        text.innerText = 'Disable 3x Loupe';
        if (currentDstPhotoUrl && loupe) {
            loupe.style.backgroundImage = "url('" + currentDstPhotoUrl + "')";
        }
    } else {
        btn.classList.remove('active', 'btn-warning');
        btn.classList.add('btn-outline-warning');
        text.innerText = 'Enable 3x Zoom Loupe';
        if (loupe) loupe.style.display = 'none';
    }
}

function updatePhotoLoupe(e) {
    if (!photoLoupeActive || !currentDstPhotoUrl) return;
    const container = document.getElementById('split-photo-container');
    const loupe = document.getElementById('photo-loupe-circle');
    if (!container || !loupe) return;

    const rect = container.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    if (x < 0 || y < 0 || x > rect.width || y > rect.height) {
        loupe.style.display = 'none';
        return;
    }

    loupe.style.display = 'block';
    loupe.style.left = x + 'px';
    loupe.style.top = y + 'px';

    const zoom = 3.0;
    loupe.style.backgroundSize = (rect.width * zoom) + 'px ' + (rect.height * zoom) + 'px';
    loupe.style.backgroundPosition = '-' + (x * zoom - 100) + 'px -' + (y * zoom - 100) + 'px';
}

document.addEventListener('DOMContentLoaded', () => {
    initSplitSliderOnce();
});


// AUTO-WIPE DEMO ANIMATION
let autoWipeActive = false;
let autoWipeTimer = null;
let autoWipeTime = 0;

function toggleAutoWipe() {
    autoWipeActive = !autoWipeActive;
    const btn = document.getElementById('btn-auto-wipe');
    const icon = document.getElementById('auto-wipe-icon');
    const text = document.getElementById('auto-wipe-text');

    if (autoWipeActive) {
        if (btn) {
            btn.classList.add('active', 'btn-info', 'text-dark');
            btn.classList.remove('btn-outline-info');
        }
        if (icon) icon.className = 'bi bi-pause-circle me-1';
        if (text) text.innerText = 'Pause Wipe';

        if (autoWipeTimer) clearInterval(autoWipeTimer);
        autoWipeTimer = setInterval(() => {
            autoWipeTime += 0.035;
            // Smooth oscillation between 18% and 82%
            const pct = 50 + 32 * Math.sin(autoWipeTime);
            setSplitPosition(pct);
        }, 30);
    } else {
        stopAutoWipe();
    }
}

function stopAutoWipe() {
    autoWipeActive = false;
    const btn = document.getElementById('btn-auto-wipe');
    const icon = document.getElementById('auto-wipe-icon');
    const text = document.getElementById('auto-wipe-text');
    if (btn) {
        btn.classList.remove('active', 'btn-info', 'text-dark');
        btn.classList.add('btn-outline-info');
    }
    if (icon) icon.className = 'bi bi-play-circle me-1';
    if (text) text.innerText = 'Auto-Wipe Demo';
    if (autoWipeTimer) {
        clearInterval(autoWipeTimer);
        autoWipeTimer = null;
    }
}


// VIDEO PLAYER SPLIT-SCREEN WIPE LOGIC
let isVidSplitDragging = false;

function setVidSplitPosition(pct) {
    pct = Math.max(0, Math.min(100, pct));
    const beforeImg = document.getElementById('vid-player-before-img');
    const divider = document.getElementById('vid-split-divider');
    const label = document.getElementById('vid-split-pos-label');

    if (beforeImg) {
        beforeImg.style.clipPath = 'polygon(0 0, ' + pct + '% 0, ' + pct + '% 100%, 0 100%)';
    }
    if (divider) {
        divider.style.left = pct + '%';
    }
    if (label) {
        label.innerText = 'Divider: ' + Math.round(pct) + '%';
    }
}

let vidSplitSliderInitialized = false;
function initVidSplitSliderOnce() {
    if (vidSplitSliderInitialized) return;
    const container = document.getElementById('vid-split-container');
    if (!container) return;
    vidSplitSliderInitialized = true;

    function handleMove(clientX) {
        const rect = container.getBoundingClientRect();
        const x = clientX - rect.left;
        const pct = (x / rect.width) * 100;
        setVidSplitPosition(pct);
    }

    container.addEventListener('mousedown', (e) => {
        isVidSplitDragging = true;
        handleMove(e.clientX);
    });

    window.addEventListener('mouseup', () => {
        isVidSplitDragging = false;
    });

    container.addEventListener('mousemove', (e) => {
        if (isVidSplitDragging) {
            handleMove(e.clientX);
        }
    });

    container.addEventListener('touchstart', (e) => {
        if (e.touches.length > 0) {
            isVidSplitDragging = true;
            handleMove(e.touches[0].clientX);
        }
    }, { passive: true });

    container.addEventListener('touchmove', (e) => {
        if (isVidSplitDragging && e.touches.length > 0) {
            handleMove(e.touches[0].clientX);
        }
    }, { passive: true });

    window.addEventListener('touchend', () => {
        isVidSplitDragging = false;
    });
}

function updateCCTVLightingControls() {
    const bEl = document.getElementById('cctv-ctrl-brightness');
    const cEl = document.getElementById('cctv-ctrl-contrast');
    const llEl = document.getElementById('cctv-ctrl-lowlight');
    const dnEl = document.getElementById('cctv-ctrl-denoise');

    const brightness = bEl ? parseInt(bEl.value) : 0;
    const contrast = cEl ? parseFloat(cEl.value) : 1.0;
    const lowlight = llEl ? llEl.checked : false;
    const denoise = dnEl ? dnEl.checked : false;

    const bValEl = document.getElementById('cctv-val-brightness');
    if (bValEl) bValEl.innerText = (brightness > 0 ? '+' : '') + brightness;

    const cValEl = document.getElementById('cctv-val-contrast');
    if (cValEl) cValEl.innerText = contrast.toFixed(1) + 'x';

    fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            brightness: brightness,
            contrast: contrast,
            low_light_boost: lowlight,
            denoise: denoise
        })
    }).catch(err => console.error(err));
}

function resetCCTVLightingControls() {
    if (document.getElementById('cctv-ctrl-brightness')) document.getElementById('cctv-ctrl-brightness').value = 0;
    if (document.getElementById('cctv-ctrl-contrast')) document.getElementById('cctv-ctrl-contrast').value = 1.0;
    if (document.getElementById('cctv-ctrl-lowlight')) document.getElementById('cctv-ctrl-lowlight').checked = false;
    if (document.getElementById('cctv-ctrl-denoise')) document.getElementById('cctv-ctrl-denoise').checked = false;
    updateCCTVLightingControls();
}
