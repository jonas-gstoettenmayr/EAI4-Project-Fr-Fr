#include "rps_preprocessor.h"
#include "consts.h"
#include "bmp_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION 
// needed to include the implementation of the stb header, 
// it's a single header library so it works like this
#include "stb_image_resize2.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>

#ifdef TIME
  #include <chrono>
#endif

namespace { 

#ifdef DEBUG
// -----------------------------
// Debug BMP saver - debug was mostly helped from chatty for obvi reasons
// ----------------------------- 
void SaveDebugImage(const std::string& name,
                    const std::vector<uint8_t>& rgb,
                    int w, int h)
{
    SaveBmp24(name, rgb, w, h);

    
    std::cout << "[DEBUG] saved " << name
              << " (" << w << "x" << h << ")\n";

}
#endif

// // Split RGB and NORMALIZE (IMPORTANT FIX)
// void SplitNormalize(const std::vector<uint8_t>& rgb, int w, int h, std::vector<float>& r, std::vector<float>& g, std::vector<float>& b){
//     size_t n = w * h;
//     r.resize(n);
//     g.resize(n);
//     b.resize(n);

//     for (size_t i = 0; i < n; i++)
//     { // same here with the 255
//         r[i] = rgb[i*3 + 0] / 255.0f;
//         g[i] = rgb[i*3 + 1] / 255.0f;
//         b[i] = rgb[i*3 + 2] / 255.0f;
//     }
// }

// Merge channels into RGB uint8
// std::vector<uint8_t> MergeRGB(const std::vector<float>& r,const std::vector<float>& g,const std::vector<float>& b){
//     size_t n = r.size();
//     std::vector<uint8_t> out(n * 3);

//     for (size_t i = 0; i < n; i++)
//     { // ik you said no more normalization 
//       // but this is the only reason it works so please leave the 255 lol
//         out[i*3+0] = (uint8_t)std::lround(std::clamp(r[i], 0.f, 1.f) * 255.f);
//         out[i*3+1] = (uint8_t)std::lround(std::clamp(g[i], 0.f, 1.f) * 255.f);
//         out[i*3+2] = (uint8_t)std::lround(std::clamp(b[i], 0.f, 1.f) * 255.f);
//     }

//     return out;
// }

}  // namespace

void PreprocessForRPS(const rpicam::RgbFrame & image, PreprocessResult & result)
{
    result.success = false;
    if (image.rgb.empty()){
        result.error_message = "empty input";
        return;
    }

    #ifdef DEBUG
        SaveDebugImage(
            "debug_01_input.bmp",
            image.rgb,
            image.width,
            image.height);
    #endif

    #ifdef TIME
      auto start = std::chrono::steady_clock::now();
    #endif

    if (image.active_width < cCropWidth || image.active_height < cCropHeight){
      result.error_message = "image two small";
      return;
    }
    
    // only extarcting the active region if specified, otherwise use the whole image
    std::vector<pixel> cropped(cCropWidth * cCropHeight * 3);
    if (image.active_width != cCropWidth || image.active_height != cCropHeight){
      int crop_x = (image.active_width - cCropWidth) /2;
      int crop_y = image.active_height ? image.active_y : 0;
      
      // Cache row sizes in bytes
      size_t bytes_per_row_dst = cCropWidth * 3;
      
      // Only rows loop and copy, faster
      for (int y = 0; y < image.active_height; y++) {
        // starting memory address for the source row and destination row
        
        const pixel * src_row_ptr = &image.rgb[((y + crop_y) * image.width + crop_x) * 3];
        pixel * dst_row_ptr = &cropped[y * cCropWidth * 3];
        
        // block copy
        std::memcpy(dst_row_ptr, src_row_ptr, bytes_per_row_dst);
      } 

    } else {
      cropped = image.rgb;
    }
    #ifdef TIME
      auto stop = std::chrono::steady_clock::now();
      std::cout << "Cropping Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif
    

    #ifdef DEBUG
        SaveDebugImage("debug_02_crop.bmp", cropped, cCropWidth, cCropHeight);
    #endif

    #ifdef TIME
      start = std::chrono::steady_clock::now();
    #endif

    // // using stb header for resizing, it's faster than the custom implementation
    // // doesn't need a gaussian blur either, since the stb header does it internally
    // // like the aliasing effect, so it's better to use it

    stbir_resize_uint8_srgb(
        cropped.data(),  cCropWidth,  cCropHeight,  0, // 0 stride means tightly packed
        result.input.data(), cModelInputWidth, cModelInputHeight, 0, // 0 stride means tightly packed
        STBIR_RGB                                    // 3 channels: Red, Green, Blue
    );    

    #ifdef TIME
      stop = std::chrono::steady_clock::now();
      std::cout << "Resiize Bilinear with stb header Took: "<< std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() <<" ms" << std::endl;
    #endif

    #ifdef DEBUG
        SaveDebugImage(
            "debug_03_resize.bmp",
            result.input,
            cModelInputWidth,
            cModelInputHeight);
    #endif

    result.success = true;

    #ifdef DEBUG
        SaveDebugImage(
            "debug_04_final.bmp",
            result.input,
            cModelInputWidth,
            cModelInputHeight);
    #endif
}

void ConvertBMPIImageToFrame(const BmpImage & image, rpicam::RgbFrame & frame){
  frame.rgb = image.rgb;
  frame.height = image.height;
  frame.width = image.width;
  frame.active_height = image.height;
  frame.active_width = image.width;
}