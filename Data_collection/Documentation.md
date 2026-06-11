# Collection

## Setup
Run this:

``` bash
sudo apt-get install python3-pil
```

to add the Pillow library to open and save images

## Running
Run this:

``` bash
python3 ./label.py
```

to start the labelling process. First choose with bracelet (orange) or without (white). Then choose label: square - rock, triangle - sissors, rectangle - paper, line - gay. Press the same direction again, the letter will flash <span style="color:blue">blue</span>. when selected. The reocording starts after the flash. The sample has been succesfully recorded when the letter has flashed <span style="color:green">green</span>

## Files

#### label.py
Runs the image creation and labelling process. The created image gets saved in the correct directory according to the label and type chosen.  For image cretion uses the compiled take_picture binary

#### take_picture.cpp
Creates an image with the pi camera in the temp directory. Uses bmp_image and camera_capture from the container used in class. The images are 512 by 512 pixels


## Warning

If you notice a misspelling in a label name SHUT UP SHUT UP DO NOT MENTION IT


# Processing

## Running
Run this:

``` bash 
python3 ./preprocess.py
```

Creates a processed folder with the images resized. The size can be changed in process.py


## The c++ stuff

The c++ preprocess file follows the same logic as digit_preprocessor.cpp but without all the digit stuff and keep the colour. The preprocessing steps are the exact same in c++ and in python


# Augmentation

## Setup
Run this:

``` bash
sudo apt install python3-torchvision
```

To do stuff yeah

## Running

Run this:
``` bash
/usr/bin/python3 augment.py
```

Creates an augmented folder. The amount of augmented samples per image can be changed in augment.py

The augmentations are random brightness shifts and flipping