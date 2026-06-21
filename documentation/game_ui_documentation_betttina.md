# Game User Interface - Bettina PÃ¶lzleitner

Since we played rock, paper, scissors I designed the images displayed on the 8x8 LED matrix. The images displayed are:<br>

- rock
- paper
- scissors
- reset
- numbers
- camera

## Pattern Format

I wanted to use different colours in the displaying of the patters instead of just having the letters for the outcome displayed, I tried to make sure that the images are somewhat recognisable. That also meant that I had to save the colour per pixel instead of saving all the rows by defining which pixel is on and which is off. 

~~~c++
uint16_t old_pattern[1][8] = 
{
    {0x00000000,0x000100000,0x000100000,0x000100000,0x000100000,0x000100000,0x000100000,0x000000000}
}

constexpr uint16_t new_pattern[8][8] = 
{
    {0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0xffff,0xffff,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000},
}
~~~

## Rock
For the design of the rock I tried to think of how people design huge rocks in the game Minecraft as the game is basically just blocks and very similar to our LED matrix if you think 2-dimensional.

## Paper
Similar to the idea for rock I looked at the in-game icon for paper in Minecraft. I removed all the yellowish pixel and replaced them with white. Additionally I scaled the size down a bit since the icon uses more pixel than we have available.

## Scissors
For scissors I thought of a stereotypical picture of a pair of scissors which is usually the tool ready to cut something and with red handles. Thus the necessary pixels and the colour were noted down.

## Reset
As per request of the teammates, I made the reset be a rainbow. 

## Numbers
For the numbers the digits provided to us were used since we are lazy IT students.

## Camera
I thought of an old camera to use as an image for the taking of the image. The background is white as the flash should be an indicator for an image to be taken.

---
## Issues

One of the issues was that the format was different since I did not want to colour everything in the same value, but each pixel individually. This change made the displaying more annoying to work with.