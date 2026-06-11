#include "preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::vector<float> GaussianBlur3x3(const std::vector<float>& input, int width, int height) {
  std::vector<float> output(static_cast<std::size_t>(width * height), 0.0F);
  const int kernel[3][3] = {
      {1, 2, 1},
      {2, 4, 2},
      {1, 2, 1},
  };

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float sum = 0.0F;
      int weight_sum = 0;
      for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
          const int sample_x = std::clamp(x + kx, 0, width - 1);
          const int sample_y = std::clamp(y + ky, 0, height - 1);
          const int weight = kernel[ky + 1][kx + 1];
          sum += input[static_cast<std::size_t>(sample_y * width + sample_x)] * static_cast<float>(weight);
          weight_sum += weight;
        }
      }
      output[static_cast<std::size_t>(y * width + x)] = sum / static_cast<float>(weight_sum);
    }
  }

  return output;
}


std::vector<float> ResizeBilinear(const std::vector<float>& input, int input_width, int input_height, int output_width, int output_height) {
  std::vector<float> output(static_cast<std::size_t>(output_width * output_height), 0.0F);

  if (input_width <= 0 || input_height <= 0 || output_width <= 0 || output_height <= 0) {
    return output;
  }

  const float scale_x = (output_width > 1) ? static_cast<float>(input_width - 1) / static_cast<float>(output_width - 1) : 0.0F;
  const float scale_y = (output_height > 1) ? static_cast<float>(input_height - 1) / static_cast<float>(output_height - 1) : 0.0F;

  for (int y = 0; y < output_height; ++y) {
    const float src_y = scale_y * static_cast<float>(y);
    const int y0 = static_cast<int>(std::floor(src_y));
    const int y1 = std::min(y0 + 1, input_height - 1);
    const float wy = src_y - static_cast<float>(y0);

    for (int x = 0; x < output_width; ++x) {
      const float src_x = scale_x * static_cast<float>(x);
      const int x0 = static_cast<int>(std::floor(src_x));
      const int x1 = std::min(x0 + 1, input_width - 1);
      const float wx = src_x - static_cast<float>(x0);

      const float top_left = input[static_cast<std::size_t>(y0 * input_width + x0)];
      const float top_right = input[static_cast<std::size_t>(y0 * input_width + x1)];
      const float bottom_left = input[static_cast<std::size_t>(y1 * input_width + x0)];
      const float bottom_right = input[static_cast<std::size_t>(y1 * input_width + x1)];

      const float top = top_left + wx * (top_right - top_left);
      const float bottom = bottom_left + wx * (bottom_right - bottom_left);
      output[static_cast<std::size_t>(y * output_width + x)] = top + wy * (bottom - top);
    }
  }

  return output;
}

}  // namespace

HandPreprocessResult PreprocessHandImage(const BmpImage& image) {
  HandPreprocessResult result;

  if (image.width <= 0 || image.height <= 0 || image.rgb.empty()) {
    result.error_message = "Input image is empty.";
    return result;
  }

  const int w = image.width;
  const int h = image.height;
  const std::size_t pixel_count = static_cast<std::size_t>(w * h);

  // Separate into channels and normalise
  std::vector<float> chan_r(pixel_count);
  std::vector<float> chan_g(pixel_count);
  std::vector<float> chan_b(pixel_count);

  for (std::size_t i = 0; i < pixel_count; ++i) {
    chan_r[i] = static_cast<float>(image.rgb[i * 3U + 0U]) / 255.0F;
    chan_g[i] = static_cast<float>(image.rgb[i * 3U + 1U]) / 255.0F;
    chan_b[i] = static_cast<float>(image.rgb[i * 3U + 2U]) / 255.0F;
  }

  // Apply gaussian blur
  chan_r = GaussianBlur3x3(chan_r, w, h);
  chan_g = GaussianBlur3x3(chan_g, w, h);
  chan_b = GaussianBlur3x3(chan_b, w, h);

  // Resize
  const auto resized_r = ResizeBilinear(chan_r, w, h, ImageWidth, ImageHeight);
  const auto resized_g = ResizeBilinear(chan_g, w, h, ImageWidth, ImageHeight);
  const auto resized_b = ResizeBilinear(chan_b, w, h, ImageWidth, ImageHeight);

  // Convert back to one vector
  const std::size_t output_pixels =
      static_cast<std::size_t>(ImageWidth * ImageHeight);
  result.model_input.resize(output_pixels * 3U);

  for (std::size_t i = 0; i < output_pixels; ++i) {
    result.model_input[i * 3U + 0U] = std::clamp(resized_r[i], 0.0F, 1.0F);
    result.model_input[i * 3U + 1U] = std::clamp(resized_g[i], 0.0F, 1.0F);
    result.model_input[i * 3U + 2U] = std::clamp(resized_b[i], 0.0F, 1.0F);
  }

  result.success = true;
  return result;
}
