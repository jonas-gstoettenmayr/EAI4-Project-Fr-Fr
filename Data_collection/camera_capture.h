#pragma once

#include <string>

struct CaptureOptions {
  std::string output_path = "/tmp/mnist_capture.bmp";
  int timeout_ms = 60;
  int width = 512;
  int height = 512;
};

bool CaptureStillBmp(const CaptureOptions& options, std::string* backend_used, std::string* error_message);
