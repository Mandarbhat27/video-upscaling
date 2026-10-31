# Real-Time Adaptive Feedback Engine

## 1. Frame Processing Budget
For a target framerate $\text{FPS}_{\text{target}}$ (e.g. 30 FPS or 60 FPS), the per-frame computational budget is defined as:
T_{\text{budget}} = \frac{1000}{\text{FPS}_{\text{target}}} \text{ ms}
- At 30 FPS: {\text{budget}} = 33.33 \text{ ms}$.
- At 60 FPS: {\text{budget}} = 16.67 \text{ ms}$.

## 2. Feedback Control & Hysteresis
To prevent high-frequency toggling (thrashing) between interpolation methods caused by individual frame variance, the engine employs:
1. **Rolling Window Smoothing**: Tracks the rolling mean $\bar{T}_{\text{rolling}}$ over a window of  = 10$ frames.
2. **Asymmetric Thresholds**:
   - **Overload Threshold**: $\bar{T}_{\text{rolling}} > 1.05 \times T_{\text{budget}}$
   - **Upgrade / Headroom Threshold**: $\bar{T}_{\text{rolling}} < 0.65 \times T_{\text{budget}}$
3. **Consecutive Overload / Underload Verification**:
   - Downgrades require 2 consecutive overload detections.
   - Upgrades require 4 consecutive underload detections (higher hysteresis for conservative upgrade).
4. **Cooldown Hold Time**: Following any transition, an enforced cooldown period of  = 12$ frames suppresses further switching, allowing the new algorithm's latency to stabilize.

## 3. State Transition Model
`
┌─────────────────┐
│     BICUBIC     │ (High Quality, ~17ms OMP)
└────────┬────────┘
         │  Overload (Latency > 1.05 * Budget)
         ▼
┌─────────────────┐
│    BILINEAR     │ (Balanced Quality, ~5.4ms OMP)
└────────┬────────┘
         │  Severe Overload (Latency > 1.05 * Budget)
         ▼
┌─────────────────┐
│     NEAREST     │ (Fast Failsafe, ~1.6ms OMP)
└─────────────────┘
`
Transitions reverse automatically when sustained headroom is verified.
