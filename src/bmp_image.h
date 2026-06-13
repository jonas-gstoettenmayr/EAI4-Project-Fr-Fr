#pragma once

#include <vector>
#include <cstdint>

typedef uint8_t pixel;

struct BmpImage {
  int width = 0;
  int height = 0;
  std::vector<pixel> rgb;
};

BmpImage LoadBmp24(const std::string& path);