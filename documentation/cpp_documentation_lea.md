# CPP program - Lea Treml

I mostly worked on Preprocessing, especially adjusting the preprocessing from the data collection part andrej did, to the live service. As well as adjusting the tflite_rps_classifier function. 

## Before Optimization

Cropping Took: 0 ms
Normalize Took: 28 ms
Gaussian Took: 633 ms
Resize Bilinear Took: 30 ms

The first working version used an unoptimized crop, a split normalization between RGB and Gaussian, Gaussian was a self-made function that used a lot of overhead, resizing and finally merging the RGB again. This did work - however since there was a time we wanted to achieve, which was 200ms and not 700ms there had to be work done on that regard.

Why the Gaussian function was so slow: 
- running a full 3x3 convolution over the entire cropped image three times (once per channel), and then throwing 90% away with the downsizing is inefficient 
- there was also waste happing on every pixel, not just around the edges
    - it recomputed from scratch the middle pixels when in reality only the edhes changed
    - std::clamp ran 18 times per pixel... 

## After Optimization

So there obviously needed to be an adjustment to be made on at least how we approached the Gaussian, to come even close to the 200ms goal. Which turned out to using an external resource. `stb_image_resize2.h`

This helped cut down the time to a fraction - approx 17 ms (cannot verify due to not having a pi here at this moment sorry)

Because of the fact that the external library takes care to make sure to not get aliasing artifacts, we only need to call the resizing function. There is no need for Gaussian Blur OR Normalization anymore.

That is because: 
- `stbir_resize_uint8_srgb` resamples in linear light, rather than averaging raw uint8 values directly
- the output is not identical to the old pipeline, however that doesn't really matter in out case because the training data pipeline was also changed to reflect the changes in the RealTime one.

## TFLite Classifier

It's a rebrand from the DigitClassifier in the exercise-lecture part.

Most of the adjustment came down to the different input and output shape - which all used const values from the `const.h` file to be coherent.

The model has two outputs, the usual rock/paper/scissors/reset probabilities + a second sigmoid output (`win`) that says whether the round should go in the user's favor. `Predict()` reads both, picks the highest probability class via `max_element`, and thresholds `win` at 0.5. This uses the model Maria trained in RealTime. 
