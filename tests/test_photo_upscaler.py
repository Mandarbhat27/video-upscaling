import os
import sys
import unittest
import numpy as np
import cv2

curr_dir = os.path.dirname(os.path.abspath(__file__))
frontend_dir = os.path.abspath(os.path.join(curr_dir, '..', 'frontend'))
if frontend_dir not in sys.path:
    sys.path.append(frontend_dir)

from upscaler_engine import NativeUpscaler

class TestPhotoUpscaler(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.upscaler = NativeUpscaler()

    def test_pure_interpolation_vs_enhanced(self):
        # Create a test synthetic high-contrast pattern
        img = np.zeros((100, 100, 3), dtype=np.uint8)
        img[20:80, 20:80] = 255
        cv2.circle(img, (50, 50), 20, (0, 255, 0), -1)

        # 1. Pure Bicubic (enhancement = 0.0)
        pure, lat_pure = self.upscaler.upscale(
            img, scale_factor=2.0, method='BICUBIC', policy='OPENMP_STATIC',
            num_threads=4, enhancement=0.0
        )
        self.assertEqual(pure.shape, (200, 200, 3))
        self.assertGreater(lat_pure, 0.0)

        # 2. Enhanced Bicubic (enhancement = 1.0)
        enhanced, lat_enh = self.upscaler.upscale(
            img, scale_factor=2.0, method='BICUBIC', policy='OPENMP_STATIC',
            num_threads=4, enhancement=1.0
        )
        self.assertEqual(enhanced.shape, (200, 200, 3))
        self.assertGreater(lat_enh, 0.0)

        # Measure Laplacian sharpness
        g_pure = cv2.cvtColor(pure, cv2.COLOR_BGR2GRAY)
        g_enh = cv2.cvtColor(enhanced, cv2.COLOR_BGR2GRAY)
        lap_pure = cv2.Laplacian(g_pure, cv2.CV_64F).var()
        lap_enh = cv2.Laplacian(g_enh, cv2.CV_64F).var()

        # Enhanced must have significantly higher sharpness than pure bicubic
        self.assertGreater(lap_enh, lap_pure)
        gain_pct = ((lap_enh - lap_pure) / lap_pure) * 100.0
        print(f"\n   [Photo Upscaler Test] Pure Bicubic Sharpness: {lap_pure:.1f} | Enhanced Sharpness: {lap_enh:.1f} (+{gain_pct:.1f}% boost)")

if __name__ == '__main__':
    unittest.main()

