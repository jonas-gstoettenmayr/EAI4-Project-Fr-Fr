
// Build: g++ -std=c++17 -o take_picture take_picture.cpp bmp_image.cpp camera_capture.cpp
#include <filesystem>
#include <iostream>
#include <string>
#include "bmp_image.h"
#include "camera_capture.h"

int main() {

    CaptureOptions opts;
    opts.output_path = "temp/temp_img.bmp";
    opts.timeout_ms = 500;
    opts.width = 512;
    opts.height = 512;

    std::string backend, error;
    if (!CaptureStillBmp(opts, &backend, &error)) {
        std::cerr << "Capture failed: " << error << std::endl;
        return 1;
    }
    std::cout << "Saved image" << std::endl;
    return 0;
}