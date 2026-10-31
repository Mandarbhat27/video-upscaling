@echo off
title Adaptive Real-Time Video Upscaling (OpenMP)
cd /d "%~dp0frontend"
echo ============================================================
echo   Adaptive Video Upscaling Server (OpenMP + Flask)
echo   Dashboard URL: http://127.0.0.1:5000
echo ============================================================
python app.py
pause
