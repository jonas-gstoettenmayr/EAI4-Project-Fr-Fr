#include "rps_preprocessor.h"
#include "consts.h"
#include "bmp_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>

#ifdef TIME
  #include <chrono>
#endif

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
        output[i * 3U + 0U] = static_cast<pixel>(std::lround(std::clamp(r[i], 0.0F, 1.0F)));
        output[i * 3U + 1U] = static_cast<pixel>(std::lround(std::clamp(g[i], 0.0F, 1.0F)));
        output[i * 3U + 2U] = static_cast<pixel>(std::lround(std::clamp(b[i], 0.0F, 1.0F)));
    }

    return output;
}

#ifdef DEBUG
// -----------------------------
// Debug BMP saver - debug was mostly helped from chatty for obvi reasons
// ----------------------------- 
void SaveDebugImage(const std::string& name,
                    const std::vector<uint8_t>& rgb,
                    int w, int h)
{
    SaveBmp24(name, rgb, w, h);

    
    std::cout << "[DEBUG] saved " << name
              << " (" << w << "x" << h << ")\n";

}
#endif

// Split RGB and NORMALIZE (IMPORTANT FIX)
void SplitNormalize(const std::vector<uint8_t>& rgb, int w, int h, std::vector<float>& r, std::vector<float>& g, std::vector<float>& b){
    size_t n = w * h;
    r.resize(n);
    g.resize(n);
    b.resize(n);

    for (size_t i = 0; i < n; i++)
    { // same here with the 255
        r[i] = rgb[i*3 + 0] / 255.0f;
        g[i] = rgb[i*3 + 1] / 255.0f;
        b[i] = rgb[i*3 + 2] / 255.0f;
    }
}

// Merge channels into RGB uint8
std::vector<uint8_t> MergeRGB(const std::vector<float>& r,const std::vector<float>& g,const std::vector<float>& b){
    size_t n = r.size();
    std::vector<uint8_t> out(n * 3);

    for (size_t i = 0; i < n; i++)
    { // ik you said no more normalization 
      // but this is the only reason it works so please leave the 255 lol
        out[i*3+0] = (uint8_t)std::lround(std::clamp(r[i], 0.f, 1.f) * 255.f);
        out[i*3+1] = (uint8_t)std::lround(std::clamp(g[i], 0.f, 1.f) * 255.f);
        out[i*3+2] = (uint8_t)std::lround(std::clamp(b[i], 0.f, 1.f) * 255.f);
    }

    return out;
}

}  // namespace

void PreprocessForRPS(const rpicam::RgbFrame & image, PreprocessResult & result)
{
    if (image.rgb.empty()){
        result.error_message = "empty input";
        return;
    }

    #ifdef DEBUG
        SaveDebugImage(
            "debug_01_input.bmp",
            image.rgb,
            image.width,
            image.height);
    #endif

    #ifdef TIME
      auto start = std::chrono::steady_clock::now();
    #endif
    
    // only extarcting the active region if specified, otherwise use the whole image
    int crop_h = image.active_height ? image.active_height : image.height;
    int crop_w = image.active_width ? image.active_width : image.width;
    std::vector<pixel> cropped(crop_w * crop_h * 3);
    if (image.active_width != image.width || image.active_height != image.height){
      int crop_x = image.active_width ? image.active_x : 0;
      int crop_y = image.active_height ? image.active_y : 0;
      
      // Cache row sizes in bytes
      size_t bytes_per_row_dst = crop_w * 3;
      
      // Only rows loop and copy, faster
      for (int y = 0; y < crop_h; y++) {
        // starting memory address for the source row and destination row
        
        const pixel * src_row_ptr = &image.rgb[((y + crop_y) * image.width + crop_x) * 3];
        pixel * dst_row_ptr = &cropped[y * crop_w * 3];
        
        // block copy
        std::memcpy(dst_row_ptr, src_row_ptr, bytes_per_row_dst);
      } 

    } else {
      cropped = image.rgb;
    }
    #ifdef TIME
      auto stop = std::chrono::steady_clock::now();
      std::cout << "Cropping Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif
    

    #ifdef DEBUG
        SaveDebugImage("debug_02_crop.bmp", cropped, crop_w, crop_h);
    #endif

    // For Jonas: Ik you said no more normalization but 
    // legit the only reason it now works is bc of this fix
    // be mercifull *prayge*

    // This is done bc it's a lot easier to work with float
    // than with the u_int8 values
    // they are converted back later on in the process
    #ifdef TIME
      start = std::chrono::steady_clock::now();
    #endif
    std::vector<float> r,g,b;
    SplitNormalize(cropped, crop_w, crop_h, r,g,b);
    #ifdef TIME
      stop = std::chrono::steady_clock::now();
      std::cout << "Normalize Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif
    
    #ifdef TIME
      start = std::chrono::steady_clock::now();
    #endif
    r = GaussianBlur3x3(r, crop_w, crop_h);
    g = GaussianBlur3x3(g, crop_w, crop_h);
    b = GaussianBlur3x3(b, crop_w, crop_h);
    #ifdef TIME
      stop = std::chrono::steady_clock::now();
      std::cout << "Gaussian Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif

    #ifdef DEBUG
        SaveDebugImage(
            "debug_03_blur.bmp",
            MergeRGB(r,g,b),
            crop_w, crop_h);
    #endif

    // sesize to model input 
    #ifdef TIME
      start = std::chrono::steady_clock::now();
    #endif
    r = ResizeBilinear(r, crop_w, crop_h, cModelInputWidth, cModelInputHeight);
    g = ResizeBilinear(g, crop_w, crop_h, cModelInputWidth, cModelInputHeight);
    b = ResizeBilinear(b, crop_w, crop_h, cModelInputWidth, cModelInputHeight);
    #ifdef TIME
      stop = std::chrono::steady_clock::now();
      std::cout << "Resiize Bilinear Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif

    #ifdef DEBUG
        SaveDebugImage(
            "debug_04_resize.bmp",
            MergeRGB(r,g,b),
            cModelInputWidth,
            cModelInputHeight);
    #endif

    // here it goes back to uint8 and interleaved as RGB
    result.input = MergeRGB(r,g,b);
    result.success = true;

    #ifdef DEBUG
        SaveDebugImage(
            "debug_05_final.bmp",
            result.input,
            cModelInputWidth,
            cModelInputHeight);
    #endif
}

void ConvertBMPIImageToFrame(const BmpImage & image, rpicam::RgbFrame & frame){
  frame.rgb = image.rgb;
  frame.height = image.height;
  frame.width = image.width;
}