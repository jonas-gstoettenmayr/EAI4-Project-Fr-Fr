#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <cstdint>
#include <inttypes.h>
#include <cstdbool>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include "./sensehat.h"

// Led file handle
static int ledFile = -1;
static bool lowLight_switch = false;
static bool lowLight_state = false;
static uint16_t *pixelMap;
static size_t screensize = 0;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

const uint8_t gamma8[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
    4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8,
    8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 13,
    14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21,
    21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29, 30,
    31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 50, 51, 52, 54, 55, 56, 57,
    58, 59, 60, 61, 62, 63, 64, 66, 67, 68, 69, 70, 72, 73, 74,
    75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89, 90, 92, 93, 95,
    96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119,
    120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146,
    148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177,
    180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252,
    255};


// ----------------------
// Initialization
// ----------------------

// Internal. Parse input devices file and extract event file handler number.
// Returns event file handler number as int.
// Returns -1 if Sense Hat joystick is not found.
#define MAX_LINE_LENGTH 256
#define DEVICES_FILE "/proc/bus/input/devices"
#define JOYSTICK_NAME1 "rpi-sense-joy"
#define JOYSTICK_NAME2 "Raspberry Pi Sense HAT Joystick"
#define HANDLERS_KEY "Handlers"
#define EVENT_PREFIX "event"

// Internal. Parse framebuffer devices file and extract RPi-Sense FB number.
// Returns FB file number as int.
// Returns -1 if RPi-Sense FB is not found.
#define FB_FILE "/proc/fb"
#define FB_DEVICE_NAME "RPi-Sense FB"

int _getFBnum()
{
    char line[MAX_LINE_LENGTH];
    int num = -1;
    FILE *fd = fopen(FB_FILE, "r");
    bool match = false;

    if (!fd)
    {
        fprintf(stderr, "Failed to open framebuffer file: %s\n",
                strerror(errno));
        return num;
    }

    while (fgets(line, sizeof(line), fd) && !match)
    {
        line[strcspn(line, "\n")] = 0;

        if (strstr(line, FB_DEVICE_NAME))
        {
            if (sscanf(line, "%d", &num) == 1)
            {
                printf("Sense Hat LED matrix points to device /dev/fb%d\n",
                       num);
                match = true;
            }
        }
    }

    fclose(fd);

    if (num == -1)
    {
        fprintf(stderr, "Failed to find Sense Hat LED matrix device name\n");
    }

    return num;
}

// Turn off all LEDs
void senseClear()
{
    memset(pixelMap, 0, screensize);
    // wait for the LEDs to turn off -> 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// Sense Hat Initialization
// . file handles for led framebuffer and joystick
// . character set
// . IMU
// . GPIO chip
bool senseInit()
{
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo vinfo;
    // Initialisation boolean set to true by default.
    // Set to false if any initialization step goes wrong.
    bool initOk = true;
    char framebufferFilename[20] = "/dev/fb", fb_num_str[4];
    int fb_num;

    // LED matrix
    if ((fb_num = _getFBnum()) >= 0)
    {
        // Open LED matrix file descriptor
        sprintf(fb_num_str, "%hd", fb_num);
        strcat(framebufferFilename, fb_num_str);
        ledFile = open(framebufferFilename, O_RDWR);
        if (ledFile < 0)
        {
            printf("Failed to open LED frame buffer file handle.\n%s\n",
                   strerror(errno));
            initOk = false;
        }

        // Get framebuffer device identity
        if (initOk && ioctl(ledFile, FBIOGET_FSCREENINFO, &fix_info) < 0)
        {
            printf("Unable to set LED frame buffer operation.\n%s\n",
                   strerror(errno));
            initOk = false;
        }

        // Check the correct device has been found
        if (initOk && strcmp(fix_info.id, "RPi-Sense FB") != 0)
        {
            puts("RPi-Sense FB not found");
            initOk = false;
        }

        // Get screen ie LED matrix information
        if (initOk && ioctl(ledFile, FBIOGET_VSCREENINFO, &vinfo) == -1)
        {
            printf("Unable to get screen information.\n%s\n", strerror(errno));
            initOk = false;
        }

        // Print LED matrix screen size
        if (initOk)
        {
            printf("%dx%d, %dbpp\n", vinfo.xres_virtual, vinfo.yres_virtual,
                   vinfo.bits_per_pixel);

            // Figure out the size of the screen in bytes
            screensize = vinfo.xres_virtual * vinfo.yres_virtual *
                         vinfo.bits_per_pixel / 8;

            // Map the led frame buffer device into memory
            pixelMap =
                (uint16_t *)mmap(NULL, screensize, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, ledFile, (off_t)0);
            if (pixelMap == MAP_FAILED)
            {
                printf("Unable to map the LED matrix into memory.\n%s\n",
                       strerror(errno));
                initOk = false;
            }
        }

        if (!initOk)
            close(ledFile);
    }
}

// Free Sense Hat file handles
void senseShutdown()
{
    senseClear();
    munmap(pixelMap, screensize);
    // Close led I2C file handle
    if (ledFile != -1)
    {
        close(ledFile);
        ledFile = -1;
    }
}

// ----------------------
// LEDs
// ----------------------

// Internal. Reduce light intensity if lowLight_switch is true.
rgb_pixel_t _lowLightDimmer(rgb_pixel_t px)
{
    uint8_t w;

    px.color[_R] = gamma8[px.color[_R]];
    px.color[_G] = gamma8[px.color[_G]];
    px.color[_B] = gamma8[px.color[_B]];
    w = MIN(px.color[_R], MIN(px.color[_G], px.color[_B])) / 3;
    px.color[_R] -= w;
    px.color[_G] -= w;
    px.color[_B] -= w;

    return px;
}

// Lower led light intensity
void senseSetLowLight(bool low)
{
    if (low)
        lowLight_switch = true;
    else
        lowLight_switch = false;
    lowLight_state = false;
}

// Internal. Encodes [R,G,B] array into 16 bit RGB565
uint16_t sensePackPixel(rgb_pixel_t rgb)
{
    uint16_t r, g, b;

    r = (uint16_t)(rgb.color[_R] >> 3) & 0x1f;
    g = (uint16_t)(rgb.color[_G] >> 2) & 0x3f;
    b = (uint16_t)(rgb.color[_B] >> 3) & 0x1f;
    return r << 11 | g << 5 | b;
}

// Internal. Decodes 16 bit RGB565 into [R,G,B] array
rgb_pixel_t senseUnPackPixel(uint16_t rgb565)
{
    rgb_pixel_t pix;

    pix.color[_R] = (uint8_t)((rgb565 & 0xf800) >> 11) << 3; // Red
    pix.color[_G] = (uint8_t)((rgb565 & 0x7e0) >> 5) << 2;   // Green
    pix.color[_B] = (uint8_t)((rgb565 & 0x1f)) << 3;         // Blue
    return pix;
}

// Turn on a single pixel with RGB565 color format
bool senseSetRGB565pixel(int x, int y, rgb565_pixel_t rgb565)
{
    // unsigned int type casting to avoid negative values
    if ((unsigned int)x < SENSE_LED_WIDTH &&
        (unsigned int)y < SENSE_LED_WIDTH)
    {
        pixelMap[x * SENSE_LED_WIDTH + y] = rgb565;
        return true;
    }

    return false;
}

// Turn on all pixels from a RGB565 predefined map
void senseSetRGB565pixels(rgb565_pixels_t pixelArray)
{
    for (int i = 0; i < SENSE_PIXELS; i++)
    {
        int x = i / SENSE_LED_WIDTH;
        int y = i % SENSE_LED_WIDTH;
        rgb565_pixel_t rgb565 = pixelArray.array[x][y];

        if (lowLight_switch && !lowLight_state)
        {
            rgb_pixel_t temp = senseUnPackPixel(rgb565);
            temp = _lowLightDimmer(temp);
            rgb565 = sensePackPixel(temp);
        }

        pixelMap[i] = rgb565;
    }

    if (lowLight_switch && !lowLight_state)
    {
        lowLight_state = true; // the brightness of all LEDs is reduced
    }
}

// Read a single pixel color in a R, G, B array
rgb_pixel_t senseGetRGBpixel(int x, int y)
{
    rgb_pixel_t read_pixel = {0}; // black pixel initialization

    // unsigned int type casting to avoid negative values
    if ((unsigned int)x < SENSE_LED_WIDTH &&
        (unsigned int)y < SENSE_LED_WIDTH)
    {
        read_pixel = senseUnPackPixel(pixelMap[x * SENSE_LED_WIDTH + y]);
    }

    // Return black if out of bounds
    return read_pixel;
}

// Read a single pixel color in a R, G, B array
rgb_pixel_t senseGetRGBpixel(int x, int y)
{
    rgb_pixel_t read_pixel = {0}; // black pixel initialization

    // unsigned int type casting to avoid negative values
    if ((unsigned int)x < SENSE_LED_WIDTH &&
        (unsigned int)y < SENSE_LED_WIDTH)
    {
        read_pixel = senseUnPackPixel(pixelMap[x * SENSE_LED_WIDTH + y]);
    }

    // Return black if out of bounds
    return read_pixel;
}

// Returns an 8x8 array containing [R,G,B] pixels
rgb_pixels_t senseGetRGBpixels()
{
    int x, y;
    rgb_pixels_t image;

    for (y = 0; y < SENSE_LED_WIDTH; y++)
        for (x = 0; x < SENSE_LED_WIDTH; x++)
            image.array[x][y] = senseGetPixel(x, y);

    return image;
}

// Internal. 90 degrees clockwise rotation of LED matrix
rgb_pixels_t _rotate90(rgb_pixels_t pixMat)
{
    int i, j;
    rgb_pixel_t temp;

    for (i = 0; i < SENSE_LED_WIDTH / 2; i++)
        for (j = i; j < SENSE_LED_WIDTH - i - 1; j++)
        {
            temp = pixMat.array[i][j];
            pixMat.array[i][j] = pixMat.array[SENSE_LED_WIDTH - 1 - j][i];
            pixMat.array[SENSE_LED_WIDTH - 1 - j][i] =
                pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j];
            pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j] =
                pixMat.array[j][SENSE_LED_WIDTH - 1 - i];
            pixMat.array[j][SENSE_LED_WIDTH - 1 - i] = temp;
        }

    return pixMat;
}

// Internal. 180 degrees rotation of LED matrix
rgb_pixels_t _rotate180(rgb_pixels_t pixMat)
{
    int i, j;
    rgb_pixel_t temp;

    for (i = 0; i < SENSE_LED_WIDTH / 2; i++)
        for (j = 0; j < SENSE_LED_WIDTH; j++)
        {
            temp = pixMat.array[i][j];
            pixMat.array[i][j] =
                pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j];
            pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j] =
                temp;
        }

    return pixMat;
}

// Internal. 270 degrees clockwise (90 anti clockwise) rotation of LED matrix
rgb_pixels_t _rotate270(rgb_pixels_t pixMat)
{
    int i, j;
    rgb_pixel_t temp;

    for (i = 0; i < SENSE_LED_WIDTH / 2; i++)
        for (j = i; j < SENSE_LED_WIDTH - i - 1; j++)
        {
            temp = pixMat.array[i][j];
            pixMat.array[i][j] = pixMat.array[j][SENSE_LED_WIDTH - 1 - i];
            pixMat.array[j][SENSE_LED_WIDTH - 1 - i] =
                pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j];
            pixMat.array[SENSE_LED_WIDTH - 1 - i][SENSE_LED_WIDTH - 1 - j] =
                pixMat.array[SENSE_LED_WIDTH - 1 - j][i];
            pixMat.array[SENSE_LED_WIDTH - 1 - j][i] = temp;
        }

    return pixMat;
}

// Rotate 90, 180, 270 degrees clockwise
rgb_pixels_t senseRotation(unsigned int angle)
{
    rgb_pixels_t rotated;

    switch (angle)
    {
    case 90:
        rotated = senseGetPixels();
        rotated = _rotate90(rotated);
        break;
    case 180:
        rotated = senseGetPixels();
        rotated = _rotate180(rotated);
        break;
    case 270:
        rotated = senseGetPixels();
        rotated = _rotate270(rotated);
        break;
    default:
        rotated = senseGetPixels();
        break;
    }

    return rotated;
}
