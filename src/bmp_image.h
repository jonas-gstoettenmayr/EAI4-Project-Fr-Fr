#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "consts.h"

struct BmpImage {
  int width = 0;
  int height = 0;
  std::vector<pixel> rgb;
};

BmpImage LoadBmp24(const std::string& path);

void SaveBmp24(const std::string& path,
               const std::vector<pixel>& rgb,
               int width,
               int height);