#pragma once

#include <string>
#include <vector>
#include "RpiCameraCapture.hpp"

#include "bmp_image.h"

struct PreprocessResult {
  bool success = false;
  std::string error_message;
  std::vector<pixel> input;
};

// Processes the frame into the required format for the model
// flat vector of input width * input height * input channels, type pixels
// pixel is an alias for uint8_t which is an alias for unsigned char
PreprocessResult PreprocessForRPS(const rpicam::RgbFrame & image);
