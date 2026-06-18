// header for all the constants for configuration
#pragma once

#include <string>
#include <chrono>

// uncomment to enable debbuing
// #define DEBUG
#define TIME

using namespace std::literals::chrono_literals;

// type of the image headers
typedef uint8_t pixel;

// input configs
constexpr const char* cCapturePath = "/tmp/mnist_capture.bmp";
constexpr const char* cTestImagePath = "test_digit.bmp";

// display settings
constexpr std::chrono::milliseconds cShowGestureTime = 2000ms;
constexpr std::chrono::milliseconds cShowResultTime = 1000ms;

// model 
constexpr const char* cDefaultModel = "model.tflite";
constexpr size_t cSampleAmount = 5;
constexpr size_t cModelOutputs = 4;
constexpr size_t cCountDownLenght = 3;
constexpr size_t cModelInputWidth = 224;
constexpr size_t cModelInputHeight = 224; // corrected Typo here
constexpr size_t cModelInputChannels = 3;

// camera settings
constexpr std::chrono::milliseconds cCaptureTimeoutMs = 2500ms;
constexpr size_t cFPS = 5;
constexpr auto cWaitTime = std::chrono::milliseconds(1000/cFPS);
constexpr int cCaptureWidth = 512;
constexpr int cCaptureHeight = 512;
constexpr bool cShowOnSenseHat = true;

// perf settings
constexpr int cDefaultWarmupRuns = 1;
constexpr int cDefaultBenchmarkRuns = 2;