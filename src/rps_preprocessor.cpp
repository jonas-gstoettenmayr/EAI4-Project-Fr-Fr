#include "rps_preprocessor.h"
#include "consts.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace { // taken 1to1 from preprocess.cpp

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

// Blur, resize, clamp, and interleave three float channels into uint8 RGB output.
std::vector<pixel> ProcessChannels(std::vector<float> r, std::vector<float> g, std::vector<float> b, int src_width, int src_height){
    // Apply Gaussian blur to each channel.
    r = GaussianBlur3x3(r, src_width, src_height);
    g = GaussianBlur3x3(g, src_width, src_height);
    b = GaussianBlur3x3(b, src_width, src_height);

    // Resize each channel to 28x28 using bilinear interpolation.
    r = ResizeBilinear(r, src_width, src_height, cModelInputWidth, cModelInputHeight);
    g = ResizeBilinear(g, src_width, src_height, cModelInputWidth, cModelInputHeight);
    b = ResizeBilinear(b, src_width, src_height, cModelInputWidth, cModelInputHeight);

    const std::size_t kPixelCount = static_cast<std::size_t>(cModelInputWidth) * static_cast<std::size_t>(cModelInputHeight);

    // Clamp to [0,1], scale to [0,255], round, interleave as RGB.
    std::vector<pixel> output(kPixelCount * cModelInputChannels, 0);

    for (std::size_t i = 0; i < kPixelCount; ++i) {
      // clamp directly in lround
        output[i * 3U + 0U] = static_cast<pixel>(std::lround(std::clamp(r[i], 0.0F, 1.0F) * 255.0F));
        output[i * 3U + 1U] = static_cast<pixel>(std::lround(std::clamp(g[i], 0.0F, 1.0F) * 255.0F));
        output[i * 3U + 2U] = static_cast<pixel>(std::lround(std::clamp(b[i], 0.0F, 1.0F) * 255.0F));
    }

    return output;
}

// Splits a tightly-packed interleaved uint8 RGB buffer into three float
// channels normalized to [0,1]. No stride — assumes width*3 bytes per row.
void SplitInterleavedRgb(const std::vector<pixel>& rgb, int width, int height,
                          std::vector<float>* r, std::vector<float>* g, std::vector<float>* b) {
  const std::size_t kPixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  r->resize(kPixelCount);
  g->resize(kPixelCount);
  b->resize(kPixelCount);

  for (std::size_t i = 0; i < kPixelCount; ++i) {
    (*r)[i] = static_cast<float>(rgb[i * 3U + 0U]) / 255.0F;
    (*g)[i] = static_cast<float>(rgb[i * 3U + 1U]) / 255.0F;
    (*b)[i] = static_cast<float>(rgb[i * 3U + 2U]) / 255.0F;
  }
}

}  // namespace

PreprocessResult PreprocessForRPS(const rpicam::RgbFrame& image) {
  PreprocessResult result;

  if (image.width == 0 || image.height == 0 || image.rgb.empty()) {
    result.error_message = "Input frame is empty.";
    return result;
  }

  const std::size_t expected_size = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 3U;

  if (image.rgb.size() != expected_size) {
    result.error_message = "Input frame size does not match its dimensions :/.";
    return result;
  }

  // Determine the active (non-letterboxed) region. If active_width/height are unset (0), the whole frame is active.
  const unsigned int crop_x = image.active_width > 0 ? image.active_x : 0;
  const unsigned int crop_y = image.active_height > 0 ? image.active_y : 0;
  const unsigned int crop_w = image.active_width > 0 ? image.active_width : image.width;
  const unsigned int crop_h = image.active_height > 0 ? image.active_height : image.height;

  if (crop_x + crop_w > image.width || crop_y + crop_h > image.height) {
    result.error_message = "Active region exceeds frame bounds.";
    return result;
  }

  // Extract and normalize the active region into separate channels.
  std::vector<float> chan_r(static_cast<std::size_t>(crop_w) * crop_h);
  std::vector<float> chan_g(static_cast<std::size_t>(crop_w) * crop_h);
  std::vector<float> chan_b(static_cast<std::size_t>(crop_w) * crop_h);

  for (unsigned int y = 0; y < crop_h; ++y) {
    const std::size_t src_row = (static_cast<std::size_t>(crop_y + y) * image.width + crop_x) * 3U;
    const std::size_t dst_row = static_cast<std::size_t>(y) * crop_w;

    for (unsigned int x = 0; x < crop_w; ++x) {
      const std::size_t src_index = src_row + static_cast<std::size_t>(x) * 3U;
      const std::size_t dst_index = dst_row + x;

      chan_r[dst_index] = static_cast<float>(image.rgb[src_index + 0U]) / 255.0F;
      chan_g[dst_index] = static_cast<float>(image.rgb[src_index + 1U]) / 255.0F;
      chan_b[dst_index] = static_cast<float>(image.rgb[src_index + 2U]) / 255.0F;
    }
  }


  result.input = ProcessChannels(chan_r, chan_g, chan_b, static_cast<int>(crop_w), static_cast<int>(crop_h));
  result.success = true;
  return result;
}