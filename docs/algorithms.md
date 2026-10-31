# Mathematical Principles of Interpolation Algorithms

## 1. Pixel Coordinate Alignment
To avoid spatial half-pixel shifts and ensure isotropic scaling between discrete coordinate systems, continuous source coordinates are mapped using center-alignment:
x_{\text{src}} = \frac{x_{\text{dst}} + 0.5}{S_x} - 0.5
y_{\text{src}} = \frac{y_{\text{dst}} + 0.5}{S_y} - 0.5
where  = W_{\text{dst}} / W_{\text{src}}$ and  = H_{\text{dst}} / H_{\text{src}}$.

---

## 2. Nearest Neighbour Interpolation
- **Mathematical Form**:
  I_{\text{dst}}(x_{\text{dst}}, y_{\text{dst}}) = I_{\text{src}}(\text{clamp}(\text{round}(x_{\text{src}}), 0, W-1), \text{clamp}(\text{round}(y_{\text{src}}), 0, H-1))
- **Complexity**: $\mathcal{O}(1)$ operations per output pixel. 1 memory lookup per channel.
- **Visual Profile**: Preserves sharp color transitions without blurring, but produces prominent blocky staircase (aliasing) artifacts.
- **Role in Adaptive Engine**: The emergency failsafe method when computational headroom is severely constrained.

---

## 3. Bilinear Interpolation
- **Mathematical Form**:
  Let  = \lfloor x_{\text{src}} \rfloor, x_1 = x_0 + 1$ and  = \lfloor y_{\text{src}} \rfloor, y_1 = y_0 + 1$.
  Fractional distances  = x_{\text{src}} - x_0$,  = y_{\text{src}} - y_0$.
  Interpolated intensity across 4 neighbors:
  I(x, y) = (1-u)(1-v) I(x_0, y_0) + u(1-v) I(x_1, y_0) + (1-u)v I(x_0, y_1) + uv I(x_1, y_1)
- **Complexity**: 4 memory reads, 4 multiplies, 3 additions per channel per pixel.
- **Visual Profile**: Continuous first-order gradients. Eliminates blockiness, but softens high-frequency edge textures.
- **Role in Adaptive Engine**: The balanced intermediate quality mode.

---

## 4. Bicubic Interpolation (Catmull-Rom Spline)
- **Mathematical Form**:
  Evaluates 16 surrounding source pixels ( \times 4$ kernel) using Keys' third-order cubic convolution kernel ( = -0.5$):
  W(t) = \begin{cases} (a + 2)|t|^3 - (a + 3)|t|^2 + 1 & |t| \le 1 \\ a|t|^3 - 5a|t|^2 + 8a|t| - 4a & 1 < |t| < 2 \\ 0 & \text{otherwise} \end{cases}
  Separable 2D evaluation:
  I_{\text{dst}}(x, y) = \sum_{n=-1}^{2} W(n - v) \sum_{m=-1}^{2} W(m - u) \cdot I_{\text{src}}(x_0 + m, y_0 + n)
- **Complexity**: 16 memory reads, 20 floating-point multiplies per channel per output pixel.
- **Visual Profile**: High perceptual sharpness, fine edge preservation, superior contrast fidelity.
- **Role in Adaptive Engine**: Default high-fidelity target when CPU budget permits.
