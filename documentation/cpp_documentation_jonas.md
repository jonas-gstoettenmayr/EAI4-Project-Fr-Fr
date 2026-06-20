# CPP program - Jonas Gstoettenmayr

**What I Worked on:** mainly the `src` dir, organised the overall structure of the project, task design and optimisation of overall project.

## Throughflow

The executable can be called with args, for either the `--benchmark` or `--inference` mode, (the default mode is inference).

### Benchmark mode

In this mode the model is benchmarked to see how fast inference is. It starts with a configurable amount of warmup runs before running the model a configurable amount of times to see the true speed of the model after beeing loaded into RAM.

### Inference mode

The model runs in an infinite loop through the following steps:

1. Start the camera and wait a few seconds for it to adjust
1. A countdown of 3 seconds (shown on pi)
1. Caputure a frame with 680x512 (frame from live video feed)
1. Resize the frame to model input of 160x160
1. Predict the gesture
   * Mobile net backbone
   * multihead output
   * (For full breakdown see the model documentation) 
1. Repeat 5 times (from point 3) 
1. The prediction is the most voted gesture
1. The win variable is also voted on democratically
1. If win is true the model should losse (win is for the user)
1. Showing the win/loss symbol on the sensehat (deppending on the win variable)
1. Repeats at 2., unless the gay (reset) gesture is predicted upon which it ends the program

#### Live data

I used the live data structure provided by Prof. Kastner to get the live frame in the 4:3 format that the camera uses (682x512) then cut it down to the size we used to take the original data in (512x512) so that it fits the 1:1 ratio. Then we resize it using the external lib `stb_image_resize2.h` to the model input of 160x160. The external lib was used as to meet the 20fps target (as it is heavily optimised to use SIMD...) .

## Optimisations

* Using the external `stb_image_resize2.h` for efficent resizing.
* Setting heavy compiler flags
  * release mode
  * O3 optimisation
  * exact CPU version (Cortex A53 for our raspberry Pi 3)
  * TFLITE flags for optimisation
* avoidance of data copying (tried to preallocate as much data as possible and reuse that)

Metrics:

* Preprocessing -
  * ~1 ms for cropping the image
  * ~10-15 ms for resising the image
* Model ~13 ms inference
* Model size - 39193 parameter -> 1828 kb (638 for int8)

Which would allow for a stable 30 FPS (The camera does not allow it though, and we don't use it either so ¯\\_(ツ)_/¯ ).

## The service

I also designed the service, it waits until after the IP address is displayed before starting the gesture classifier on its own, it can be ended by it detecting the `gay` gesture.