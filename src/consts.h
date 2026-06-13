// header for all the constants for configuration
#pragma once

#include <string>
#include <chrono>

using namespace std::literals::chrono_literals;

constexpr const size_t cModelOutputs = 4;

// model 
constexpr const char* cDefaultModel = "model.tflite";
// input configs
constexpr const char* cCapturePath = "/tmp/mnist_capture.bmp";
constexpr const char* cTestImagePath = "test_digit.bmp";

constexpr std::chrono::milliseconds cShowGestureTime = 2000ms;
constexpr std::chrono::milliseconds cShowResultTime = 1000ms;

constexpr size_t cSampleAmount = 5;
constexpr size_t cClasses = 4;
constexpr size_t cCountDownLenght = 3;


constexpr std::chrono::milliseconds cCaptureTimeoutMs = 2500ms;
constexpr size_t cFPS = 25;
constexpr auto cWaitTime = std::chrono::milliseconds(1000/cFPS);
constexpr int cCaptureWidth = 512;
constexpr int cCaptureHeight = 512;
constexpr bool cShowOnSenseHat = true;

constexpr int cDefaultWarmupRuns = 20;
constexpr int cDefaultBenchmarkRuns = 1000;