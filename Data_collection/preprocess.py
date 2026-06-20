# _____________________________________________________________________________________________
# Imports
# _____________________________________________________________________________________________
import numpy as np
from PIL import Image
from scipy.ndimage import convolve
import shutil
import os


# _____________________________________________________________________________________________
# Variables
# _____________________________________________________________________________________________

Width = 224
Height = 224

# Kernel = np.array([
#     [1, 2, 1],
#     [2, 4, 2],
#     [1, 2, 1],
# ], dtype=np.float32) / 16.0

source = "data"
destination = "processed"

files = []


# _____________________________________________________________________________________________
# Functions
# _____________________________________________________________________________________________

# def gaussian_blur(channel):

#     return convolve(channel, Kernel, mode = 'nearest')


# def resize_bilinear(channel, out_w, out_h):

#     in_h, in_w = channel.shape

#     # Build output coordinate grids
#     xs = np.linspace(0, in_w - 1, out_w)
#     ys = np.linspace(0, in_h - 1, out_h)
#     grid_x, grid_y = np.meshgrid(xs, ys)

#     x0 = np.floor(grid_x).astype(int)
#     y0 = np.floor(grid_y).astype(int)
#     x1 = np.clip(x0 + 1, 0, in_w - 1)
#     y1 = np.clip(y0 + 1, 0, in_h - 1)
#     wx = grid_x - x0
#     wy = grid_y - y0

#     top = channel[y0, x0] + wx * (channel[y0, x1] - channel[y0, x0])
#     bottom = channel[y1, x0] + wx * (channel[y1, x1] - channel[y1, x0])
#     return top + wy * (bottom - top)


def preprocess_image(path):
    img = Image.open(path).convert("RGB")
    img = img.resize((Width, Height), Image.Resampling.LANCZOS) # the stuff from stb_image_resize2.h

    return img

def list_files_recursive(path = '.'):

    for entry in os.listdir(path):
        full_path = os.path.join(path, entry)

        if os.path.isdir(full_path):
            list_files_recursive(full_path)

        else:
            files.append(full_path)
    
    return files


def make_get_files():

    shutil.copytree(source, destination)

    files = list_files_recursive(destination)

    return files



def main():
    
    files = make_get_files()

    for image_path in files:

        new_img = preprocess_image(image_path)

        new_img.save(image_path)

main()