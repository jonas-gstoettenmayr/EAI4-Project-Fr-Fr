# Dataloader

As we are training a Multihead Model, we needed to make sure that the Data matches up between the Gestures and the Conditions (for each X one Gesture and one Condition). Therefore there is a function in the Dataloader that does exactly this, but gives me only the path to the image and the Classes based on the folderstructure. 

I also made a little function that gives us the labels with the way they are assigned (without to 0 aso). They are seperate for the two heads so if we change it we could easily change it for everybody to just look at them files.

# Train and Convert

I added some arguments to the parser, like Early Stopping patience, lr reduce patience and min lr, that I also used for the model. 

## Plots

There is a function for the training loss & accuracy that gives us a plot of both as a picture to check how the model is doing, if it is overfitting/underfitting or doing something else.

There is also a function that makes two confusion matrices, once for Gestures and once for Acessories also for us to see how it is doing (helped trough issues like it just random guessing, just guessing one class aso.)

## Data

Next up there are some more Data functions. First up the split train validation was reworked and is now kind of not fully usefull as I just use the train_test_split from sklearn. In main one can see that I call it on x and y gestures and x and y conditions seperatly, but this works as I am using a random seed and it therefore giving me the right combination for both of them. Like this I have one Image and the two gestures together and split in the same way. 

Next up is a function that reads the images based on a path, makes it usable for the network and for safety rescales it again. 

Afterwards is the augmentation function from Andrej

The make Dataset uses the two functions above and creates a dataset out of all the images in test, validation and train. The Augmentation is right now set to false everywhere as this yields the best results for us.

## Modeling

The Model gave me a lot of issues. It has a Mobilenet backbone and the trainable parameter gave me some issues as in the beginning it showed me that it was doing good but as soon as you call the model again it was doing bad. Right now it is using a trainable backbone which is not defined with backbone.trainable=True which works. Another issue was figuring out what image it actually expects as I got conflicting information about that -> it actually expects with include_preprocessing [0,255] and without [-1,1]. Now it works.
Definition in the function:

    Shared MobileNetV3Small backbone with two independent classification heads.
    Head 1 (primary): Dense(64) -> Dropout -> Dense(16) -> Dense(num_classes1)
    Head 2 (primary 2): Dense(2) -> Dense(1)
    Both heads are trained jointly.

Both heads count the same (1 and 1) for the loss metric. One has a multiclass output and the other a binary with sigmoid. 

## Evaluation

The representative dataset, export tflite, quantize for tflite and dequantize for tflite are the same functions as the ones we used in class, they did not need a rework to work.

Evaluate Tflite works on the same idea, but it was made more complicated with the two heads and them heads having different names in the tflite export they evaluate the test images if they are correctly classified seperatly, once for the gesture and once for the acessory condition. 

Evaluate Keras, parameter count and append models again worked without an issue.

Because I was doing lazy loading for memory I also made a path to numpy function that literally loads a set of image paths into a numpy array.

There is also a class called MemoryCallback as I wanted to check it for each epoch and I had some memory issues. 

## Main

In the main firstly the artifacts_dir gets created if it does not already exist, then the Data gets loaded with the function from the dataloader, afterwards it gets split like already mentioned and is made into a dataset with make_dataset.

Afterhead the multihead is created and fit to the data. It also got some callbacks: My memoryCallback which tells me about the RAM usage, the ReduceLROnOlateau from Keras which reduces the lr so that it can search in a good area closer and the Early Stopping from Keras based on the combined val_loss from the two heads.

Afterhead we set the multihead.trainable to false so that it can be converted to tflite. 

Next up we are saving the training history, appending the model and a int8 version of it (spoiler its not to good) to out models list and are evaluating them. before being exported to tflite and tested on them. 

Then our model_metrics is being created with all the evaluations.

In the end we save the multihead model thats in the normal stand as model.tflite and save the metrics.


# Why we did not do the pruning and students

The mobilenet made it complicated for us, mobilenet itself is very good already but it also makes it harder for us to prune anything and would have exploded the scope if I tried to make all of them functions work (the functions we made in the exercise would not work with it). Same for the students I tried some out but they were all bad, the only feasable thing would have been using a mobilenet with a smaller alpha and trying to knowledge distill to it and we decided against that as it worked good. 


# Model_metrics.csv:

| tflite_file           | method               | tflite_mode | validation_accuracy_g | test_accuracy_g | validation_accuracy_c | test_accuracy_c | tflite_test_accuracy_g | tflite_test_accuracy_c | tflite_file_size_kilobytes | parameters_total |
| --------------------- | -------------------- | ----------- | --------------------: | --------------: | --------------------: | --------------: | ---------------------: | ---------------------: | -------------------------: | ---------------: |
| multihead.tflite      | normal FP32 training | fp32        |                 0.939 |           1.000 |                 0.960 |           0.970 |                  1.000 |                  0.970 |                       3795 |            39193 |
| multihead-int8.tflite | now in int8          | int8        |                 0.939 |           1.000 |                 0.960 |           0.970 |                  0.697 |                  0.879 |                       1225 |            39193 |


