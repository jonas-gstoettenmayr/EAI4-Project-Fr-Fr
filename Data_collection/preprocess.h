#pragma once

#include <string>
#include <vector>

#include "bmp_image.h"

constexpr int ImageWidth = 64;
constexpr int ImageHeight = 64;

struct PreprocessResult {
  bool success = false;
  std::string error_message;

  std::vector<float> model_input;
};

PreprocessResult PreprocessImage(const BmpImage& image);
