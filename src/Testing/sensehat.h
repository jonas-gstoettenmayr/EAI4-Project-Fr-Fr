#ifndef __SENSEHAT_H__
#define __SENSEHAT_H__

#include <cstdint>
#include <cstdbool>

// Number of colors for a unique pixel
#define COLORS 3
// Red color index
#define _R 0
// Green color index
#define _G 1
// Blue color index
#define _B 2
// LED matrix width
#define SENSE_LED_WIDTH 8
// Number of pixels in the bitmap
#define SENSE_PIXELS (SENSE_LED_WIDTH * SENSE_LED_WIDTH)

/// \brief color attributes of a pixel encoded in an integer of rgb565_pixel_t
/// type \details RGB565 format represents the 3 colors in a 16 bit integer
/// \details The bits are arranged this way: RRRRRGGGGGGBBBBB
typedef uint16_t rgb565_pixel_t;

/// \brief led matrix 2 dimensional array of pixels encoded in rgb565_pixel_t
/// type
typedef struct
{
    rgb565_pixel_t array[SENSE_LED_WIDTH][SENSE_LED_WIDTH];
} rgb565_pixels_t;

/// \brief color attributes of a pixel encoded in an array of 3 bytes
/// \details the 3 bytes are in R, G, B order
typedef struct
{
    uint8_t color[COLORS];
} rgb_pixel_t;

/// \brief led matrix 2 dimensional array with pixels encoded in rgb_pixel_t
/// type
typedef struct
{
    rgb_pixel_t array[SENSE_LED_WIDTH][SENSE_LED_WIDTH];
} rgb_pixels_t;

#define GPIO_CONSUMER "SenseHatLib"

/// \brief Initialize file handles and communications
/// \details led matrix framebuffer, josytick input, IMU calibration
/// parameters, character set, GPIO chip
/// \return bool false if something went wrong
bool senseInit();

/// \brief Close file handles and communications with Sense HAT
void senseShutdown();

/// \brief Clear LED store and shut all the LEDs
void senseClear();

/// \brief Lower LED light intensity
/// \param[in] low true if color values must be lowered to limit power
/// consomption
void senseSetLowLight(bool low);

/// \brief Write the color attributes of one single pixel in RGB565 format
/// \param[in] x row number [0..7]
/// \param[in] y column number [0..7]
/// \return bool false if something went wrong
bool senseSetRGB565pixel(int x, int y, rgb565_pixel_t rgb565);

/// \brief Write the color attributes of one single pixel with the 3 RGB values
const auto senseSetPixel = senseSetRGB565pixel;

/// \brief Write color attributes of all pixels at once
/// \param rgb565_map 2 dimensional array of integers in RGB565 format:
/// rgb565_pixel_t type
void senseSetRGB565pixels(rgb565_pixels_t rgb565_map);

/// \brief Set all pixels with the same color attributes
const auto senseSetPixels = senseSetRGB565pixels;

/// \brief Read the color attributes of one single pixel from its coordinates
/// \param[in] x row number [0..7]
/// \param[in] y column number [0..7]
/// \return color attributes of the pixel encoded in a array of 3 bytes:
/// rgb_pixel_t type
rgb_pixel_t senseGetRGBpixel(int x, int y);

/// \brief Read the color attributes of one single pixel from its coordinates
const auto senseGetPixel = senseGetRGBpixel;

/// \brief Read color attibutes of all pixels at once
/// \return 2 dimensional array of RGB color attributes of rgb_pixel_t type
rgb_pixels_t senseGetRGBpixels();

/// \brief Read color attibutes of all pixels at once
const auto senseGetPixels = senseGetRGBpixels;

#endif // __SENSEHAT_H__
