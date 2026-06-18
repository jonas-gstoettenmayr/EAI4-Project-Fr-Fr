#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include "consts.h"
#include "bmp_image.h"
#include "rps_preprocessor.h"
#include "sense_hat_display.h"
#include "tflite_rps_classifier.h"
#include "RpiCameraCapture.hpp"

namespace {

// Selects which use case should run.
enum class ProgramMode {
  cCameraRPSInference,
  cBenchmarkRPSModel,
};

// Stores command line options.
struct ProgramOptions {
  std::string model_path = cDefaultModel;
  std::string test_image_path = cTestImagePath;
  ProgramMode mode = ProgramMode::cCameraRPSInference;
  bool show_on_sense_hat = cShowOnSenseHat;
  int warmup_runs = cDefaultWarmupRuns;
  int benchmark_runs = cDefaultBenchmarkRuns;
};

// Prints the command line usage.
void PrintUsage(const char* program_name) {
  std::cerr
      << "Usage:\n"
      << "  " << program_name << " --camera --model "<< cDefaultModel <<"\n"
      << "  " << program_name << " --benchmark --model model.tflite --test-image test_digit.bmp --runs 1000 --warmup 20\n"
      << "\n"
      << "Examples:\n"
      << "  " << program_name << " --camera --model artifacts/digit_float32.tflite\n"
      << "  " << program_name << " --camera --model artifacts/digit_int8.tflite\n"
      << "  " << program_name << " --benchmark --model artifacts/synapse_pruned.tflite\n"
      << "  " << program_name << " --benchmark --model artifacts/channel_pruned.tflite\n"
      << "  " << program_name << " --benchmark --model artifacts/neuron_pruned.tflite\n";
}

// Parses a positive integer option value.
bool ParsePositiveInt(const std::string& text, int* value) {
  try {
    const int parsed = std::stoi(text);

    if (parsed <= 0) {
      return false;
    }

    *value = parsed;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// Parses all command line arguments.
bool ParseArgs(int argc, char** argv, ProgramOptions* options) {
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];

    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return false;
    }

    if (arg == "--camera") {
      options->mode = ProgramMode::cCameraRPSInference;
      continue;
    }

    if (arg == "--benchmark") {
      options->mode = ProgramMode::cBenchmarkRPSModel;
      continue;
    }

    if (arg == "--model" && index + 1 < argc) {
      options->model_path = argv[++index];
      continue;
    }

    if (arg == "--test-image" && index + 1 < argc) {
      options->test_image_path = argv[++index];
      continue;
    }

    if (arg == "--runs" && index + 1 < argc) {
      if (!ParsePositiveInt(argv[++index], &options->benchmark_runs)) {
        std::cerr << "Invalid --runs value.\n";
        return false;
      }

      continue;
    }

    if (arg == "--warmup" && index + 1 < argc) {
      if (!ParsePositiveInt(argv[++index], &options->warmup_runs)) {
        std::cerr << "Invalid --warmup value.\n";
        return false;
      }

      continue;
    }

    if (arg == "--no-sensehat") {
      options->show_on_sense_hat = false;
      continue;
    }

    // Keep the old workflow: ./program model.tflite
    if (!arg.empty() && arg[0] != '-') {
      options->model_path = arg;
      continue;
    }

    std::cerr << "Unknown or incomplete argument: " << arg << "\n";
    PrintUsage(argv[0]);
    return false;
  }

  return true;
}

// Loads and preprocesses one digit image.
bool LoadRPSInput(const std::string& image_path,
                    std::vector<uint8_t>* rps_input) {
  BmpImage image;

  try {
    image = LoadBmp24(image_path);
  } catch (const std::exception& ex) {
    std::cerr << "Failed to load image: " << ex.what() << "\n";
    return false;
  }
  rpicam::RgbFrame frame;
  ConvertBMPIImageToFrame(image, frame);
  PreprocessResult preprocess;
  PreprocessForRPS(frame, preprocess);

  if (!preprocess.success) {
    std::cerr << "Preprocessing failed: " << preprocess.error_message << "\n";
    return false;
  }

  *rps_input = preprocess.input;
  return true;
}

// Benchmarks one digit model.
int RunRPSBenchmark(const ProgramOptions& options,
                      TfliteRPSClassifier* classifier) {
  std::vector<uint8_t> rps_input;

  if (!LoadRPSInput(options.test_image_path, &rps_input)) {
    return 1;
  }

  RPSPrediction prediction;

  // Run warmup inferences before measuring.
  for (int index = 0; index < options.warmup_runs; ++index) {
    prediction = classifier->Predict(rps_input);

    if (!classifier->ok()) {
      std::cerr << "Warmup inference failed: "
                << classifier->error_message() << "\n";
      return 1;
    }
  }

  // Measure repeated inference runtime.
  const auto start = std::chrono::steady_clock::now();

  for (int index = 0; index < options.benchmark_runs; ++index) {
    prediction = classifier->Predict(rps_input);
    std::cout << "Prediction: " << ConvertPredToRPS(prediction.rps) << "\n";
    std::cout << "Win: " << prediction.win << "\n" <<std::endl;

    if (!classifier->ok()) {
      std::cerr << "Inference failed: "
                << classifier->error_message() << "\n";
      return 1;
    }
  }

  const auto end = std::chrono::steady_clock::now();

  const double total_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  const double average_ms =
      total_ms / static_cast<double>(options.benchmark_runs);

  std::cout << "Digit benchmark mode\n";
  std::cout << "Model: " << options.model_path << "\n";
  std::cout << "Test image: " << options.test_image_path << "\n";
  std::cout << "Warmup runs: " << options.warmup_runs << "\n";
  std::cout << "Runs: " << options.benchmark_runs << "\n";
  std::cout << "Predicted gesture: " << prediction.rps << "\n";
  std::cout << "Confidence: " << prediction.confidence << "\n";
  std::cout << "Total inference ms: " << total_ms << "\n";
  std::cout << "Average inference ms: " << average_ms << "\n";

  return 0;
}

// Runs digit inference from the camera.
int RunCameraRPSInference(const ProgramOptions& options,
                            TfliteRPSClassifier* classifier) {
  SenseHatDisplay display;

  rpicam::CaptureParameters params;
  params.width = cCaptureWidth;
  params.height = cCaptureHeight;
  
  params.shutter_us = 0;      // set for manual shutter, e.g. 8000
  params.gain = 0.0f;         // increase for dark environments
  params.buffer_count = 20;
  params.awb = true;
  params.fps = cFPS;
  
  rpicam::RpiCameraCapture camera(params);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(cCaptureTimeoutMs));
  std::cout << "Starting Loop" << std::endl;
  try {
    while (true) {
      double mean = 0.0;
      double mod_mean = 0.0;
      double proc_mean = 0.0;
      
      // Get current image to framebuffer.
      display.StartCountDown(cCountDownLenght);
      
      std::array<RPSPrediction, cSampleAmount> preds;
      auto next_tick = std::chrono::steady_clock::now(); // current time
      PreprocessResult processed;
      for(size_t i = 0; i < cSampleAmount; i++) {
        next_tick += cWaitTime;
        auto start = std::chrono::steady_clock::now();

        std::shared_ptr<const rpicam::RgbFrame> frame = camera.currentFrame();
        while (!frame) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
          frame = camera.currentFrame();
        }
        std::cout << "frame siize" << frame->rgb.size() << std::flush;
        auto proc_start = std::chrono::steady_clock::now();
        PreprocessForRPS(* frame, processed);
        if (!processed.success){
          std::cerr << "Error: " << processed.error_message << "\n";
          return 1;
        }
        auto stop = std::chrono::steady_clock::now();
        proc_mean += std::chrono::duration_cast<std::chrono::milliseconds>(stop-proc_start).count();

        // Run outcome prediction.
        auto mod_start = std::chrono::steady_clock::now();
        preds[i] = classifier->Predict(processed.input);
        if (!classifier->ok()) {
          std::cerr << "Inference failed: "
                    << classifier->error_message() << "\n";
          return 1;
        }
        stop = std::chrono::steady_clock::now();
        mod_mean += std::chrono::duration_cast<std::chrono::milliseconds>(stop-mod_start).count();

        std::cout << "Predicted gesture: " << ConvertPredToRPS(preds[i].rps) << "\n";
        std::cout << "Confidence: " << preds[i].confidence << "\n";

        stop = std::chrono::steady_clock::now();
        mean += std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count();
        std::this_thread::sleep_until(next_tick);
      } // end for
      std::cout << "Mean Prediction/processing time: " << mean/cSampleAmount << std::endl;
      std::cout << "Mean Prediction time: " << mod_mean/cSampleAmount << std::endl;
      std::cout << "Mean Processing time: " << proc_mean/cSampleAmount << std::endl;

      // democracy calculations
      std::array<size_t, cModelOutputs> predCounts = {};
      size_t most_frequent_idx = 0;
      int win = 0;
      float AVGConf = 0.0;

      for(size_t i = 0; i < cSampleAmount; i++) {
        size_t currentRPS = preds[i].rps;
        if(++predCounts[currentRPS] > predCounts[most_frequent_idx])
        most_frequent_idx = currentRPS;
        win += (2 * preds[i].win) - 1;
        AVGConf += preds[i].confidence;
      }

      AVGConf /= cSampleAmount;

      // Show the predicted digit on the Sense HAT.
      if (options.show_on_sense_hat) {
        if (display.available()) {

          if(RPS const outcome = ConvertPredToRPS(most_frequent_idx); outcome == RPS::reset){
            display.ShowRPS(outcome, AVGConf);
            std::this_thread::sleep_for(cShowGestureTime);
            break;
          }
          if(win > 0){
            display.ShowRPS(ConvertPredToRPS((most_frequent_idx +2 ) % 3 ), AVGConf);
            std::this_thread::sleep_for(cShowGestureTime);
            display.ShowLoss();
          } else {
            display.ShowRPS(ConvertPredToRPS((most_frequent_idx +1) % 3 ), AVGConf);
            std::this_thread::sleep_for(cShowGestureTime);
            display.ShowWin();
          }
          std::this_thread::sleep_for(cShowResultTime);

        } else {
          std::cerr << "Sense HAT unavailable: "
                    << display.error_message() << "\n";
        }
      } // end if

    } // end while

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}


}  // namespace

int main(int argc, char** argv) {
  ProgramOptions options;

  // Parse command line options.
  if (!ParseArgs(argc, argv, &options)) {
    return 1;
  }

  // Load the digit model, which may be normal, pruned, float32, or int8.
  TfliteRPSClassifier classifier(options.model_path);
  if (!classifier.ok()) {
    std::cerr << "Failed to load digit model: "
              << classifier.error_message() << "\n";
    return 1;
  }
  switch (options.mode) {
    case ProgramMode::cCameraRPSInference: {
      return RunCameraRPSInference(options, &classifier);
    }
    case ProgramMode::cBenchmarkRPSModel: {
      return RunRPSBenchmark(options, &classifier);
    }

  }

  return 1;
}

// int main(int argc, char** argv) {

// }