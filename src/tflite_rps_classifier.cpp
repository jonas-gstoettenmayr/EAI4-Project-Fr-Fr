#include "tflite_rps_classifier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/interpreter_builder.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"

namespace {

// Checks whether the tensor type is supported.
bool IsSupportedTensorType(TfLiteType type) {
  return type == kTfLiteFloat32 || type == kTfLiteInt8;
}

// Converts tensor types to readable names for error messages.
const char* TensorTypeName(TfLiteType type) {
  switch (type) {
    case kTfLiteFloat32:
      return "float32";
    case kTfLiteInt8:
      return "int8";
    default:
      return "unsupported";
  }
}

// Converts a float input value to int8.
int8_t QuantizeInt8(float value, float scale, int zero_point) {
  int quantized = static_cast<int>(
      std::lround(value / scale + static_cast<float>(zero_point)));

  const int min_value =
      static_cast<int>(std::numeric_limits<int8_t>::min());
  const int max_value =
      static_cast<int>(std::numeric_limits<int8_t>::max());

  if (quantized < min_value) {
    quantized = min_value;
  }

  if (quantized > max_value) {
    quantized = max_value;
  }

  return static_cast<int8_t>(quantized);
}

// Converts an int8 output value back to float.
float DequantizeInt8(int8_t value, float scale, int zero_point) {
  return scale * static_cast<float>(static_cast<int>(value) - zero_point);
}

}  // namespace

struct TfliteRPSClassifier::Impl {
  std::unique_ptr<tflite::FlatBufferModel> model;
  std::unique_ptr<tflite::Interpreter> interpreter;
};

// Creates the classifier and loads the given TensorFlow Lite model.
TfliteRPSClassifier::TfliteRPSClassifier(const std::string& model_path)
    : impl_(std::make_unique<Impl>()) {
  ok_ = Load(model_path);
}

// Releases classifier resources.
TfliteRPSClassifier::~TfliteRPSClassifier() = default;

// Loads the model, creates the interpreter, and prepares tensors.
bool TfliteRPSClassifier::Load(const std::string& model_path) {
  // Load the .tflite model from disk.
  impl_->model = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());

  if (!impl_->model) {
    error_message_ = "Failed to load model: " + model_path;
    return false;
  }

  // Create the TensorFlow Lite interpreter.
  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*impl_->model, resolver);

  if (builder(&impl_->interpreter) != kTfLiteOk || !impl_->interpreter) {
    error_message_ = "Failed to create TensorFlow Lite interpreter.";
    return false;
  }

  // Use one CPU thread for predictable inference.
  impl_->interpreter->SetNumThreads(1);

  // Allocate memory for all tensors.
  if (impl_->interpreter->AllocateTensors() != kTfLiteOk) {
    error_message_ = "Failed to allocate tensors.";
    return false;
  }

  // Require exactly one input tensor.
  if (impl_->interpreter->inputs().size() != 1) {
    error_message_ = "The model must have exactly one input tensor.";
    return false;
  }

  // Require exactly one output tensor.
  if (impl_->interpreter->outputs().size() != 1) {
    error_message_ = "The model must have exactly one output tensor.";
    return false;
  }

  // Get the input and output tensors.
  const TfLiteTensor* input = impl_->interpreter->input_tensor(0);
  const TfLiteTensor* output = impl_->interpreter->output_tensor(0);

  // Check that the input tensor is float32 or int8.
  if (input == nullptr || !IsSupportedTensorType(input->type)) {
    error_message_ = "The model input must be float32 or int8; got ";
    error_message_ += input == nullptr ? "null" : TensorTypeName(input->type);
    error_message_ += ".";
    return false;
  }

  // Check that the output tensor is float32 or int8.
  if (output == nullptr || !IsSupportedTensorType(output->type)) {
    error_message_ = "The model output must be float32 or int8; got ";
    error_message_ += output == nullptr ? "null" : TensorTypeName(output->type);
    error_message_ += ".";
    return false;
  }

  // Check for the expected input shape [1, cModelInputHeight, cModelInputWidth, cModelInputChannels].
  if (input->dims == nullptr || input->dims->size != 4 ||
      input->dims->data[0] != 1 || 
      input->dims->data[1] != cModelInputHeight ||
      input->dims->data[2] != cModelInputWidth || 
      input->dims->data[3] != cModelInputChannels) {
    error_message_ = "Expected input shape [1, cModelInputHeight, cModelInputWidth, cModelInputChannels].";
    return false;
  }

  // Check for the expected output shape [1, 4].
  if (output->dims == nullptr || output->dims->size != 2 ||
      output->dims->data[0] != 1 || output->dims->data[1] != static_cast<int>(cModelOutputs)) {
    error_message_ = "Expected output shape [1, 4].";
    return false;
  }

  return true;
}

// Copies float input data into the model input tensor.
bool TfliteRPSClassifier::CopyInput(const std::vector<pixel>& image) {
  constexpr std::size_t kExpectedSize = cModelInputWidth * cModelInputHeight * cModelInputChannels;
  
  // Check that the input contains exactly the ModelInputWidth * cModelInputHeight * cModelInputChannels values.
  if (image.size() != kExpectedSize) {
    error_message_ = "Expected " + std::to_string(kExpectedSize) + " input pixels, got " + std::to_string(image.size()) + ".";
    return false;
  }

  // Get the model input tensor.
  TfLiteTensor* input_tensor = impl_->interpreter->input_tensor(0);

  // Normalize uint8 [0,255] → float [0,1] for float32 models.
  if (input_tensor->type == kTfLiteFloat32) {
    float* input = impl_->interpreter->typed_input_tensor<float>(0);

    for (std::size_t index = 0; index < image.size(); ++index) {
      input[index] = static_cast<float>(image[index]) / 255.0F;
    }

    return true;
  }

  // Normalize uint8 [0,255] → float [0,1] for float32 models.
  if (input_tensor->type == kTfLiteInt8) {
    int8_t * input = impl_->interpreter->typed_input_tensor<int8_t>(0);
    
    for (std::size_t index = 0; index < image.size(); ++index) {
      input[index] = QuantizeInt8(
        static_cast<float>(image[index]) / 255.0F,
        input_tensor->params.scale,
        input_tensor->params.zero_point);
    }

    return true;
  }

  // This should not happen because Load validates the tensor type.
  error_message_ = "Unsupported input tensor type.";
  return false;
}

// Reads the model output tensor and converts probabilities to floats.
Probabilities TfliteRPSClassifier::ReadOutput() const {
  Probabilities probabilities;

  // Get the model output tensor.
  const TfLiteTensor* output_tensor = impl_->interpreter->output_tensor(0);

  // Read float values directly for float32 models.
  if (output_tensor->type == kTfLiteFloat32) {
    const float* output = impl_->interpreter->typed_output_tensor<float>(0);
    std::copy(output, output + static_cast<size_t>(cModelOutputs), probabilities.begin());
    return probabilities;
  }

  // Dequantize int8 values before returning them.
  if (output_tensor->type == kTfLiteInt8) {
    const int8_t * output = impl_->interpreter->typed_output_tensor<int8_t>(0);

    for (std::size_t index = 0; index < probabilities.size(); ++index) {
      probabilities[index] = DequantizeInt8(
        output[index], 
        output_tensor->params.scale,
        output_tensor->params.zero_point);
    }

    return probabilities;
  }

  // This should not happen because Load validates the tensor type.
  return probabilities;
}

// Runs inference and returns the most likely outcome prediction.
RPSPrediction TfliteRPSClassifier::Predict(const std::vector<pixel>& image) {
  RPSPrediction prediction; // Default prediction is reset (3), win=true with 0 confidence.

  // Return an empty prediction if loading failed.
  if (!ok_) {
    return prediction;
  }

  // Copy the input image into the TensorFlow Lite input tensor.
  if (!CopyInput(image)) {
    ok_ = false;
    return prediction;
  }

  // Run inference.
  if (impl_->interpreter->Invoke() != kTfLiteOk) {
    error_message_ = "TensorFlow Lite invocation failed.";
    ok_ = false;
    return prediction;
  }

  // Read the model output as float probabilities.
  prediction.probabilities = ReadOutput();

  // Find the outcome with the highest probability.
  const auto best = std::max_element(
      prediction.probabilities.begin(),
      prediction.probabilities.end());

  prediction.rps = static_cast<int>(std::distance(prediction.probabilities.begin(), best));
  prediction.confidence = *best;

  // For now, we always set win to true. 
  // In the future, this could be determined based on the model output or other logic.
  // Don't think I can do that rn? Or am I stupid
  prediction.win = true;

  return prediction;
}

// Maps model output index to RPS enum.
RPS ConvertPredToRPS(int pred) {
  switch (pred) {
    case 0:  return RPS::rock;
    case 1:  return RPS::paper;
    case 2:  return RPS::scissors;
    default: return RPS::reset;
  }
}