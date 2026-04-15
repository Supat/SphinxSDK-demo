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
  // 21 hand landmarks in source-image pixel coordinates: [x0,y0, x1,y1, ...].
  float landmarks_xy[42];
  int src_width;
  int src_height;
  float forearm_axis_deg;  // copied from the estimator for overlay drawing
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

// Renders the source frame with skeleton + angle overlay and calls cv::imshow.
// Pumps the GUI event loop with a short waitKey. Safe to call every frame.
void ShowWristPreview(const uint8_t* image, int width, int height, int channels,
                      const WristResult& r,
                      const char* window_name = "Wrist Preview");
