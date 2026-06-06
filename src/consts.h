// header for all the constants for configuration
#pragma once

#include <string>

constexpr const size_t cModelOutputs = 4;

// model 
constexpr const char* cDefaultModel = "model.tflite";
// input configs
constexpr const char* cCapturePath = "/tmp/mnist_capture.bmp";
constexpr const char* cTestImagePath = "test_digit.bmp";

constexpr int cCaptureTimeoutMs = 1200;
constexpr int cCaptureWidth = 640;
constexpr int cCaptureHeight = 480;
constexpr bool cShowOnSenseHat = true;

constexpr int cDefaultWarmupRuns = 20;
constexpr int cDefaultBenchmarkRuns = 1000;