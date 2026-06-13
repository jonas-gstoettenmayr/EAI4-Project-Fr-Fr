#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <chrono>
#include "rps.h"

class SenseHatDisplay {
 public:
  SenseHatDisplay();
  ~SenseHatDisplay();

  bool available() const { return available_; }
  const std::string& error_message() const { return error_message_; }

  // start is countdown, e.g. start=3 -> 3, 2, 1,
  void StartCountDown(size_t start) ; 
  bool ShowRPS(RPS rps, float confidence);
  void ShowWin();
  void ShowLoss();
  void ShowPrideFlag();
  
  void ShowErrorMarker();
  void Clear();

 private:
  bool OpenFramebuffer();
  void WritePattern(const std::uint8_t pattern[8], std::uint16_t color);
  std::uint16_t MakeRgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) const;

  int file_descriptor_ = -1;
  std::size_t mapping_size_ = 0;
  unsigned char* framebuffer_ = nullptr;
  int line_length_ = 0;
  bool available_ = false;
  std::string error_message_;
};
