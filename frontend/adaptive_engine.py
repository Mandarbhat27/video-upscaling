import time
from collections import deque

class AdaptiveEngine:
    def __init__(self, target_fps=30.0, window_size=10, cooldown_frames=12):
        self.target_fps = target_fps
        self.target_budget_ms = 1000.0 / target_fps
        self.window_size = window_size
        self.cooldown_frames = cooldown_frames
        self.frames_since_last_switch = cooldown_frames
        
        self.adaptive_enabled = True
        self.current_method = 'BICUBIC'
        self.workload_level = 'NORMAL' # NORMAL, INCREASED, HIGH
        
        self.latency_history = deque(maxlen=window_size)
        self.decision_history = []
        
        self.consecutive_overloads = 0
        self.consecutive_underloads = 0
        self.algorithm_switches = 0
        self.dropped_frames = 0
        self.last_measured_latency_ms = 0.0

        # Hysteresis parameters
        self.overload_ratio = 1.05  # Latency > 105% budget triggers downgrade
        self.upgrade_ratio = 0.65   # Latency < 65% budget triggers upgrade

    def set_target_fps(self, fps):
        self.target_fps = max(1.0, float(fps))
        self.target_budget_ms = 1000.0 / self.target_fps

    def set_manual_method(self, method):
        self.current_method = method.upper()
        self.frames_since_last_switch = 0

    def set_adaptive_enabled(self, enabled):
        self.adaptive_enabled = bool(enabled)

    def set_workload_mode(self, mode):
        self.workload_level = mode.upper()

    def update_and_evaluate(self, frame_idx, frame_latency_ms):
        self.last_measured_latency_ms = frame_latency_ms
        self.latency_history.append(frame_latency_ms)
        self.frames_since_last_switch += 1

        if not self.adaptive_enabled or len(self.latency_history) < 3:
            return self.current_method

        rolling_avg = sum(self.latency_history) / len(self.latency_history)

        # Respect cooldown to prevent thrashing
        if self.frames_since_last_switch < self.cooldown_frames:
            return self.current_method

        # Case 1: Overload Detection
        if rolling_avg > (self.target_budget_ms * self.overload_ratio):
            self.consecutive_overloads += 1
            self.consecutive_underloads = 0

            # 2 consecutive frames in overload triggers downgrade
            if self.consecutive_overloads >= 2:
                prev_method = self.current_method
                switched = False
                reason = ''

                if self.current_method == 'BICUBIC':
                    self.current_method = 'BILINEAR'
                    reason = f'Overload: rolling latency ({rolling_avg:.1f}ms) > budget ({self.target_budget_ms:.1f}ms). Downgrading to Bilinear.'
                    switched = True
                elif self.current_method == 'BILINEAR':
                    self.current_method = 'NEAREST'
                    reason = f'Severe overload: rolling latency ({rolling_avg:.1f}ms) > budget ({self.target_budget_ms:.1f}ms). Downgrading to Nearest.'
                    switched = True

                if switched:
                    self.algorithm_switches += 1
                    self.frames_since_last_switch = 0
                    self.consecutive_overloads = 0
                    self.decision_history.append({
                        'frame_number': frame_idx,
                        'previous_method': prev_method,
                        'new_method': self.current_method,
                        'measured_latency_ms': round(frame_latency_ms, 2),
                        'rolling_avg_latency_ms': round(rolling_avg, 2),
                        'target_budget_ms': round(self.target_budget_ms, 2),
                        'reason': reason,
                        'timestamp': time.strftime('%H:%M:%S')
                    })

        # Case 2: Underload / Headroom Available
        elif rolling_avg < (self.target_budget_ms * self.upgrade_ratio):
            self.consecutive_underloads += 1
            self.consecutive_overloads = 0

            # 4 consecutive underload checks required for upgrade (hysteresis)
            if self.consecutive_underloads >= 4:
                prev_method = self.current_method
                switched = False
                reason = ''

                if self.current_method == 'NEAREST':
                    self.current_method = 'BILINEAR'
                    reason = f'Headroom: rolling latency ({rolling_avg:.1f}ms) << budget ({self.target_budget_ms:.1f}ms). Upgrading to Bilinear.'
                    switched = True
                elif self.current_method == 'BILINEAR':
                    self.current_method = 'BICUBIC'
                    reason = f'Sustained Headroom: rolling latency ({rolling_avg:.1f}ms) << budget ({self.target_budget_ms:.1f}ms). Upgrading to Bicubic.'
                    switched = True

                if switched:
                    self.algorithm_switches += 1
                    self.frames_since_last_switch = 0
                    self.consecutive_underloads = 0
                    self.decision_history.append({
                        'frame_number': frame_idx,
                        'previous_method': prev_method,
                        'new_method': self.current_method,
                        'measured_latency_ms': round(frame_latency_ms, 2),
                        'rolling_avg_latency_ms': round(rolling_avg, 2),
                        'target_budget_ms': round(self.target_budget_ms, 2),
                        'reason': reason,
                        'timestamp': time.strftime('%H:%M:%S')
                    })
        else:
            self.consecutive_overloads = 0
            self.consecutive_underloads = 0

        return self.current_method

    def get_status(self):
        rolling_avg = (sum(self.latency_history) / len(self.latency_history)) if self.latency_history else 0.0
        rolling_fps = (1000.0 / rolling_avg) if rolling_avg > 0.01 else 0.0
        current_fps = (1000.0 / self.last_measured_latency_ms) if self.last_measured_latency_ms > 0.01 else 0.0
        
        is_overloaded = rolling_avg > self.target_budget_ms
        if not self.adaptive_enabled:
            state_msg = f'Fixed Quality ({self.current_method})'
        elif is_overloaded:
            state_msg = 'Processing Overload (Adapting)'
        elif rolling_avg < self.target_budget_ms * self.upgrade_ratio:
            state_msg = 'Optimal Headroom'
        else:
            state_msg = 'Meeting Target (Stable)'

        return {
            'target_fps': self.target_fps,
            'target_budget_ms': round(self.target_budget_ms, 2),
            'current_method': self.current_method,
            'current_latency_ms': round(self.last_measured_latency_ms, 2),
            'current_fps': round(current_fps, 1),
            'rolling_avg_latency_ms': round(rolling_avg, 2),
            'rolling_avg_fps': round(rolling_fps, 1),
            'algorithm_switches': self.algorithm_switches,
            'dropped_frames': self.dropped_frames,
            'is_overloaded': is_overloaded,
            'state_message': state_msg,
            'workload_level': self.workload_level,
            'adaptive_enabled': self.adaptive_enabled
        }
