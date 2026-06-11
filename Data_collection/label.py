from sense_hat import SenseHat
import time
import subprocess
from PIL import Image
import datetime
import os

sense = SenseHat()

w = (255, 255, 255)
n = (0, 0, 0)
r = (255, 0, 0)
g = (0, 255, 0)
b = (0, 0, 255)
o = (255, 120, 0)

choose1 = [
    n, n, n, w, w, n, n, n,
    n, n, n, w, w, n, n, n,
    n, n, n, w, w, n, n, n,
    w, w, n, n, n, n, w, w,
    w, n, n, n, n, n, w, w,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, w, w, n, n, n,
    ]

choose2 = [
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    o, o, n, n, n, n, w, w,
    o, o, n, n, n, n, w, w,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    ]

flash = [
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    w, w, w, w, w, w, w, w,
    ]

nothing = [
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    n, n, n, n, n, n, n, n,
    ]

def label_record(current_label, recording, loop):

    while True:

        time.sleep(0.05)

        events = sense.stick.get_events()
        for event in events:
            if event.action != "pressed":
                continue

            
            if event.direction == "middle":
                loop = False
                sense.show_message("Quit", scroll_speed = 0.028)
                sense.clear()
                return current_label, recording, loop


            if event.direction == "left" and current_label == "sissors":
                recording = True
                sense.show_letter("S", text_colour = b)
                time.sleep(1)
                return current_label, recording, loop
            
            if event.direction == "left" and current_label != "sissors":     
                sense.show_letter("S")
                time.sleep(1)
                current_label = "sissors"
                break


            if event.direction == "right" and current_label == "rock":
                recording = True
                sense.show_letter("R", text_colour = b)
                time.sleep(1)
                return current_label, recording, loop
            
            if event.direction == "right" and current_label != "rock":   
                sense.show_letter("R")
                time.sleep(1)
                current_label = "rock"
                break


            if event.direction == "up" and current_label == "paper":
                recording = True
                sense.show_letter("P", text_colour = b)
                time.sleep(1)
                return current_label, recording, loop

            if event.direction == "up" and current_label != "paper":   
                sense.show_letter("P")
                time.sleep(1)
                current_label = "paper"
                break


            if event.direction == "down" and current_label == "gay":
                recording = True
                sense.show_letter("G", text_colour = b)
                time.sleep(1)
                return current_label, recording, loop
            
            if event.direction == "down" and current_label != "gay":
                sense.show_letter("G")
                time.sleep(1)
                current_label = "gay"
                break

def create_image(current_label, recording, type):

    sense.set_pixels(flash)
    time.sleep(0.2)
    sense.set_pixels(nothing)
    measuring = subprocess.Popen(["./take_picture"])
    measuring.wait()
    print(f"Recorded {current_label}")

    if current_label == "sissors":
        sense.show_letter("S", text_colour = g)

    if current_label == "rock":
        sense.show_letter("R", text_colour = g)

    if current_label == "paper":
        sense.show_letter("P", text_colour = g)

    if current_label == "gay":
        sense.show_letter("G", text_colour = g)

    img = Image.open("temp/temp_img.bmp")

    img_file_path = f"data/{type}/{current_label}/{datetime.datetime.now().replace(microsecond=0)}_{current_label}.bmp"

    img.save(img_file_path)

    print(f"Wrote {current_label}")

    os.remove("temp/temp_img.bmp")

    return

def choose_type():
     while True:
        events = sense.stick.get_events()
        for event in events:
            if event.action != "pressed":
                continue

            if event.direction == "left":
                    return "with"

            if event.direction == "right":
                    return "without"
     

def main():
    
    sense.set_pixels(choose2)
    type = choose_type()

    loop = True    
    while loop:
        current_label = None
        recording = False

        sense.set_pixels(choose1)
        
        current_label, recording, loop = label_record(current_label, recording, loop)   
        
        
        if loop == False: break

        time.sleep(0.4)
        create_image(current_label, recording, type)
        
        print("Recording and saving complete")

        time.sleep(2)

main()
