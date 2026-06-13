#pragma once

#include <memory>
#include <string>
#include <vector>
#include "rps.h"
#include "consts.h"

using Probabilities = std::array<float, cModelOutputs>;

struct RPSPrediction {
  int rps = 3; // 0rock, 1paper, 2sissors, 3reset
  bool win = true;
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
  RPSPrediction Predict(const std::vector<pixel>& image);

 private:
  struct Impl;

  bool Load(const std::string& model_path);
  bool CopyInput(const std::vector<pixel>& image);
  Probabilities ReadOutput() const;

  std::unique_ptr<Impl> impl_;
  bool ok_ = false;
  std::string error_message_;
};

// intake the pure model output index, e.g. 0 and ouptut RPS::ROCK
RPS ConvertPredToRPS(int pred); 
