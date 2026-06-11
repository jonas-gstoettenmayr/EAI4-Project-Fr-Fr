# _____________________________________________________________________________________________
# Imports
# _____________________________________________________________________________________________
from PIL import Image
import shutil
import os
from torchvision import transforms

# _____________________________________________________________________________________________
# Variables
# _____________________________________________________________________________________________

source = "/home/kit-17/Documents/EAI/Project/processed/"
destination = "/home/kit-17/Documents/EAI/Project/augmented/"

files = []

aug = transforms.Compose([
        transforms.RandomHorizontalFlip(),
        #transforms.RandomRotation(10),
        transforms.ColorJitter(brightness = 0.2)
    ])

augment_per_image = 2  # how many augmented samples per image

# _____________________________________________________________________________________________
# Functions
# _____________________________________________________________________________________________

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

def augment_save(files, aug, number):

    for file in files:

            img = Image.open(file).convert("RGB")
            stem, ext = os.path.splitext(file)

            for i in range(number):
                aug_img = aug(img)

                save_path = f"{stem}_aug{i+1}{ext}"
                aug_img.save(save_path)


def main():
    files = make_get_files()

    augment_save(files, aug, augment_per_image)

main()