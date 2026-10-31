import sys
sys.path.append(r'C:\Users\Mandar\.gemini\antigravity-ide\scratch\adaptive-video-upscaling\frontend')
from adaptive_engine import AdaptiveEngine

def test_adaptive_switching():
    print('=== TESTING ADAPTIVE ENGINE SWITCHING LOGIC ===')
    engine = AdaptiveEngine(target_fps=30.0, window_size=5, cooldown_frames=5)
    
    # Target budget should be 33.33ms
    assert abs(engine.target_budget_ms - 33.33) < 0.1, 'Budget calculation incorrect'
    print(f'[PASS] Target FPS: 30.0 -> Frame Budget: {engine.target_budget_ms:.2f}ms')
    
    # 1. Normal operating condition (latency ~18ms << 33.33ms)
    print('\n[SCENARIO 1] Normal operation under budget (Bicubic)...')
    for f in range(1, 10):
        m = engine.update_and_evaluate(f, 18.0)
    assert m == 'BICUBIC', 'Should remain BICUBIC when under budget'
    print(f'[PASS] Method remains {m} with stable 18.0ms latency')
    
    # 2. Overload condition (latency spikes to 50ms > 33.33ms budget)
    print('\n[SCENARIO 2] Overload injected (50ms > 33.33ms)...')
    # Outlier on single frame should NOT trigger immediate switch (hysteresis test)
    engine.update_and_evaluate(10, 50.0)
    assert engine.current_method == 'BICUBIC', 'Single frame outlier should not trigger switch'
    print('[PASS] Single outlier frame ignored (Hysteresis check passed)')
    
    # Continued overload
    for f in range(11, 25):
        m = engine.update_and_evaluate(f, 50.0)
    
    print(f'[STATUS] After overload, current method: {engine.current_method}')
    assert engine.current_method in ['BILINEAR', 'NEAREST'], 'Should downgrade due to sustained overload'
    print(f'[PASS] Engine downgraded to {engine.current_method}')
    
    # 3. Recovery condition (latency drops to 3.0ms << 33.33ms)
    print('\n[SCENARIO 3] Recovery phase: latency drops to 3.0ms...')
    for f in range(25, 60):
        m = engine.update_and_evaluate(f, 3.0)
    
    print(f'[STATUS] After recovery, current method: {engine.current_method}')
    assert engine.current_method == 'BICUBIC', 'Should upgrade back to BICUBIC after sustained recovery'
    print(f'[PASS] Engine recovered back to {engine.current_method}')
    
    # 4. Check decision log
    decisions = engine.decision_history
    print(f'\n[PASS] Total decisions recorded: {len(decisions)}')
    for idx, d in enumerate(decisions, 1):
        fn = d['frame_number']
        pm = d['previous_method']
        nm = d['new_method']
        rs = d['reason']
        print(f'   Decision {idx}: Frame {fn} | {pm} -> {nm} | Reason: {rs}')
    
    assert len(decisions) >= 2, 'Should have logged at least downgrade and upgrade decisions'
    print('\n=== ALL ADAPTIVE ENGINE TESTS PASSED SUCCESSFULLY! ===')

if __name__ == '__main__':
    test_adaptive_switching()
