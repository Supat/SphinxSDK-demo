#pragma once
#include <cstdint>
#include <string>

struct WristResult {
  bool valid;
  float angle_deg;
  const char* class_name;
  float confidence;
  uint64_t frame;
  uint64_t timestamp;
};

class WristEstimator {
public:
  WristEstimator(const std::string& model_path,
                 float forearm_axis_deg = 0.0f,
                 float neutral_threshold_deg = 5.0f,
                 bool flip_sign = false);
  ~WristEstimator();

  WristEstimator(const WristEstimator&) = delete;
  WristEstimator& operator=(const WristEstimator&) = delete;

  // image is 1-channel grayscale (channels=1) or 3-channel BGR (channels=3).
  WristResult Estimate(const uint8_t* image, int width, int height, int channels,
                       uint64_t frame, uint64_t timestamp);

  std::string ToJson(const WristResult& r) const;

private:
  struct Impl;
  Impl* p_;
};
