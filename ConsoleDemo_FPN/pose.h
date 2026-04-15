#pragma once
#include <cstdint>
#include <string>

struct ForearmResult {
  bool valid;
  float angle_deg;       // image-plane direction of elbow->wrist, +X=0, +Y up
  const char* source;    // "left_forearm" | "right_forearm" | "none"
  float confidence;      // min visibility of elbow & wrist after sigmoid
  // Source-image pixel coords (only meaningful when valid).
  float elbow_x, elbow_y;
  float wrist_x, wrist_y;
};

class PoseEstimator {
public:
  explicit PoseEstimator(const std::string& model_path,
                         float min_visibility = 0.5f);
  ~PoseEstimator();
  PoseEstimator(const PoseEstimator&) = delete;
  PoseEstimator& operator=(const PoseEstimator&) = delete;

  // image: 1-channel grayscale or 3-channel BGR.
  ForearmResult Estimate(const uint8_t* image, int width, int height, int channels);

private:
  struct Impl;
  Impl* p_;
};
