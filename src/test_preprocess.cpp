#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <cstring>

#include "stb_image_resize2.h"
#include "bmp_image.h"
#include "rps_preprocessor.h"
#include "consts.h"
#include "RpiCameraCapture.hpp"

// Minimal BMP writer for 24-bit RGB output.
// Mirrors the logic from save_test_digit_bmp in the training script.
void SaveBmp24(const std::string& path, const std::vector<pixel>& rgb,
               int width, int height) {
  const int row_stride = (width * 3 + 3) & ~3;  // padded to 4 bytes
  const int pixel_data_size = row_stride * height;
  const int file_size = 54 + pixel_data_size;

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open output file: " + path);
  }

  // BMP file header (14 bytes)
  out.put('B'); out.put('M');
  out.write(reinterpret_cast<const char*>(&file_size), 4);
  const int reserved = 0;
  out.write(reinterpret_cast<const char*>(&reserved), 4);
  const int pixel_offset = 54;
  out.write(reinterpret_cast<const char*>(&pixel_offset), 4);

  // DIB header (40 bytes)
  const int header_size = 40;
  out.write(reinterpret_cast<const char*>(&header_size), 4);
  out.write(reinterpret_cast<const char*>(&width), 4);
  // Negative height = top-down row order, matches how we store pixels.
  const int neg_height = -height;
  out.write(reinterpret_cast<const char*>(&neg_height), 4);
  const short planes = 1;
  out.write(reinterpret_cast<const char*>(&planes), 2);
  const short bpp = 24;
  out.write(reinterpret_cast<const char*>(&bpp), 2);
  const int compression = 0;
  out.write(reinterpret_cast<const char*>(&compression), 4);
  out.write(reinterpret_cast<const char*>(&pixel_data_size), 4);
  const int zero = 0;
  out.write(reinterpret_cast<const char*>(&zero), 4);  // x pixels/meter
  out.write(reinterpret_cast<const char*>(&zero), 4);  // y pixels/meter
  out.write(reinterpret_cast<const char*>(&zero), 4);  // colors in table
  out.write(reinterpret_cast<const char*>(&zero), 4);  // important colors

  // Pixel data — BMP stores BGR, our buffer is RGB, so swap R and B.
  const std::vector<pixel> padding(static_cast<std::size_t>(row_stride - width * 3), 0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t i = static_cast<std::size_t>(y * width + x) * 3U;
      out.put(static_cast<char>(rgb[i + 2U]));  // B
      out.put(static_cast<char>(rgb[i + 1U]));  // G
      out.put(static_cast<char>(rgb[i + 0U]));  // R
    }
    out.write(reinterpret_cast<const char*>(padding.data()),
              static_cast<std::streamsize>(padding.size()));
  }
}

int main2(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " input.bmp output.bmp\n";
    return 1;
  }

  const std::string input_path  = argv[1];
  const std::string output_path = argv[2];

  // Load the input BMP.
  BmpImage image;
  try {
    image = LoadBmp24(input_path);
  } catch (const std::exception& ex) {
    std::cerr << "Failed to load image: " << ex.what() << "\n";
    return 1;
  }

  std::cout << "Loaded: " << input_path << "\n";
  std::cout << "Input size: " << image.width << "x" << image.height << "\n";

  // Wrap BmpImage in RgbFrame — no copy of pixel data needed since
  // BmpImage.rgb and RgbFrame.rgb are both std::vector<uint8_t>.
  rpicam::RgbFrame frame;
  frame.width  = static_cast<unsigned int>(image.width);
  frame.height = static_cast<unsigned int>(image.height);
  frame.stride = static_cast<unsigned int>(image.width) * 3U;
  frame.rgb    = image.rgb;  // copy — both are std::vector<pixel>
  // active_* left at 0 → whole frame is treated as active region.

  // Run preprocessing.
  PreprocessResult result;
  PreprocessForRPS(frame, result);
  std::cout << "Actual output size: "
          << result.input.size()
          << '\n';

  int min_val = 255;
  int max_val = 0;

  for (uint8_t p : result.input) {
      min_val = std::min(min_val, (int)p);
      max_val = std::max(max_val, (int)p);
  }

  std::cout << "Min: " << min_val
            << " Max: " << max_val
            << '\n';

  if (!result.success) {
    std::cerr << "Preprocessing failed: " << result.error_message << "\n";
    return 1;
  }

  // Sanity check output size.
  constexpr std::size_t kExpected =
      static_cast<std::size_t>(cModelInputWidth) *
      static_cast<std::size_t>(cModelInputHeight) *
      static_cast<std::size_t>(cModelInputChannels);

  if (result.input.size() != kExpected) {
    std::cerr << "Unexpected output size: " << result.input.size()
              << " expected " << kExpected << "\n";
    return 1;
  }

  std::cout << "Output size: " << cModelInputWidth << "x"
            << cModelInputHeight << "x" << cModelInputChannels
            << " (" << result.input.size() << " bytes)\n";

  // Write preprocessed output to BMP for visual inspection.
  try {
    SaveBmp24(output_path, result.input, cModelInputWidth, cModelInputHeight);
  } catch (const std::exception& ex) {
    std::cerr << "Failed to save output: " << ex.what() << "\n";
    return 1;
  }

  std::cout << "Saved: " << output_path << "\n";
  std::cout << "Open both BMPs and compare visually against Andrey's Python output.\n";
  return 0;
}