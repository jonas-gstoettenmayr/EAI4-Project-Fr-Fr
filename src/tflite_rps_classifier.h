#pragma once

#include <memory>
#include <string>
#include <vector>
#include "rps.h"
#include "consts.h"

using Probabilities = std::array<float, cModelOutputs>;

struct RPSPrediction {
  RPS rps = RPS::reset;
  float confidence = 0.0F;
  Probabilities probabilities;
};

class TfliteRPSClassifier {
 public:
  explicit TfliteRPSClassifier(const std::string& model_path);
  ~TfliteRPSClassifier();

  TfliteRPSClassifier(const TfliteRPSClassifier&) = delete;
  TfliteRPSClassifier& operator=(const TfliteRPSClassifier&) = delete;
  TfliteRPSClassifier(TfliteRPSClassifier&&) = delete;
  TfliteRPSClassifier& operator=(TfliteRPSClassifier&&) = delete;

  bool ok() const { return ok_; }
  const std::string& error_message() const { return error_message_; }
  RPSPrediction Predict(const std::vector<float>& normalized_image_28x28);

 private:
  struct Impl;

  bool Load(const std::string& model_path);
  bool CopyInput(const std::vector<float>& normalized_image_28x28);
  Probabilities ReadOutput() const;

  std::unique_ptr<Impl> impl_;
  bool ok_ = false;
  std::string error_message_;
};
