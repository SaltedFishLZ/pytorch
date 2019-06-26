#include <ATen/ATen.h>
#include <ATen/NativeFunctions.h>

/* FakeQuantize Op for PerTensorAffine quantization scheme */
namespace at {
namespace native {

/* Fake-quantizes the 'inputs' tensor.
Args:
  X: Forward input tensor.
  dY: Backward input tensor (_backward op only).
  scale: scale of per tensor affine quantization
  zero_point: zero_point of per tensor affine quantization
  quant_min: minimum quantized value
  quant_max: maximum quantized value
  quant_delay: Count of global steps for which to delay the quantization.
               See note below.
  iter: The current quantization iteration used for `quant_delay`.
Returns:
  Quantized tensor (double dtype).

Notes:
  - quant_delay might be set to non-zero to help weights stabilize in the
    beginning of the training.
  - quantization range [quant_min, quant_max]
*/
Tensor fake_quantize_per_tensor_affine_cpu(
      const Tensor& self,
      double scale,
      int64_t zero_point,
      int64_t quant_min,
      int64_t quant_max,
      int64_t quant_delay,
      int64_t iter
    ) {
    // Sanity checks.
    TORCH_CHECK(self.scalar_type() == ScalarType::Float);
    if (quant_min > quant_max) {
      throw std::invalid_argument("`quant_min` should be less than or equal to `quant_max`.");
    }
    if (zero_point < 0) {
      throw std::invalid_argument("`zero_point` must be a positive integer.");
    }
    if (quant_delay < 0) {
      throw std::invalid_argument("`quant_delay` must be a positive integer.");
    }

    if (quant_delay != 0 && iter < 0) {
      throw std::invalid_argument(
        "`iter` must be >=0 for non-zero `quant_delay`");
    }

    auto Y = at::empty_like(self);

    if (quant_delay > 0 && iter <= quant_delay) {
      Y.copy_(self);  // We might want to just return the input here.
      return Y;
    }

    double inv_scale = 1.0f / scale;
    Y = (((self * inv_scale + 0.5f).floor() + zero_point)
      .clamp_min(quant_min).clamp_max(quant_max) - zero_point) * scale;
    return Y;
}

/* Backward path to fake-quantize the 'inputs' tensor.

Args:
  X: Forward input tensor.
  dY: Backward input tensor.
  scale: scale of per tensor affine quantization
  zero_point: zero_point of per tensor affine quantization
  quant_min: minimum quantized value
  quant_max: maximum quantized value
  quant_delay: Count of global steps for which to delay the quantization.
               See note in forward.
  iter: The current quantization iteration used for `quant_delay`.
Returns:
  Quantized tensor (double dtype).

Notes:
  - quant_delay might be set to non-zero to help weights stabilize in the
    beginning of the training.
  - quantization range [0, 2^bits - 1]
*/
Tensor fake_quantize_per_tensor_affine_backward_cpu(
      const Tensor& dY,
      const Tensor& X,
      double scale,
      int64_t zero_point,
      int64_t quant_min,
      int64_t quant_max,
      int64_t quant_delay,
      int64_t iter) {
    // Sanity checks.
    if (quant_min > quant_max) {
      throw std::invalid_argument("`quant_min` should be less than or equal to `quant_max`.");
    }
    if (zero_point < 0) {
      throw std::invalid_argument("`zero_point` must be a positive integer.");
    }
    if (quant_delay < 0) {
      throw std::invalid_argument("`quant_delay` must be a positive integer.");
    }
    if (X.numel() <= 0) {
      throw std::length_error("`X` is empty");
    }
    if (X.numel() != dY.numel()) {
      throw std::invalid_argument("`X` and `dY` are not the same size");
    }

    if (quant_delay != 0 && iter < 0) {
      throw std::invalid_argument(
        "`iter` must be >=0 for non-zero `quant_delay`");
    }

    auto dX = at::zeros_like(dY);
    if (quant_delay > 0 && iter <= quant_delay) {
      dX.copy_(dY);
      return dX;
    }

    double inv_scale = 1.0f / scale;
    at::Tensor Xq = (X * inv_scale + 0.5).floor() + zero_point;
    at::Tensor mask_min = (Xq >= quant_min);
    at::Tensor mask_max = (Xq <= quant_max);
    at::Tensor mask = mask_min * mask_max;
    dX = mask.type_as(dY) * dY;
    return dX;
}
}}  // namespace at::native
