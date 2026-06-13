#pragma once

#include <string>
#include <vector>
#include "RpiCameraCapture.hpp"

#include "bmp_image.h"

struct PreprocessResult {
  bool success = false;
  std::string error_message;
  bool used_dark_foreground = true;
  float threshold = 0.5F;
  int component_area = 0;
  std::vector<pixel> input;
};

PreprocessResult PreprocessForRPS(const rpicam::RgbFrame & image);
