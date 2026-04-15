#include "wrist.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr int kInputSize = 224;
constexpr int kNumLandmarks = 21;
constexpr int kWristIdx = 0;
constexpr int kMiddleMcpIdx = 9;

float NormalizeAngle180(float deg) {
  while (deg > 180.0f) deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

}  // namespace

struct WristEstimator::Impl {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "wrist"};
  Ort::SessionOptions opts;
  std::unique_ptr<Ort::Session> session;
  Ort::AllocatorWithDefaultOptions alloc;

  std::string input_name;
  std::vector<std::string> output_names;
  std::vector<int64_t> input_shape;  // expected [1, H, W, 3] or [1, 3, H, W]
  bool nhwc = true;
  int landmark_output_idx = 0;
  int score_output_idx = -1;

  float forearm_axis_deg;
  float neutral_threshold_deg;
  bool flip_sign;

  Impl(const std::string& model_path, float axis, float thresh, bool flip)
      : forearm_axis_deg(axis), neutral_threshold_deg(thresh), flip_sign(flip) {
    opts.SetIntraOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    std::wstring wpath(model_path.begin(), model_path.end());
    session = std::make_unique<Ort::Session>(env, wpath.c_str(), opts);

    auto in_name_alloc = session->GetInputNameAllocated(0, alloc);
    input_name = in_name_alloc.get();

    auto type_info = session->GetInputTypeInfo(0);
    auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
    input_shape = tensor_info.GetShape();
    if (input_shape.size() == 4 && input_shape[1] == 3) nhwc = false;

    size_t n_out = session->GetOutputCount();
    for (size_t i = 0; i < n_out; ++i) {
      auto n = session->GetOutputNameAllocated(i, alloc);
      output_names.emplace_back(n.get());
    }
    // heuristic: 21x3=63-element output is the landmarks; a 1-element output is the score.
    for (size_t i = 0; i < n_out; ++i) {
      auto info = session->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo();
      auto shp = info.GetShape();
      int64_t total = 1;
      for (auto d : shp) total *= (d > 0 ? d : 1);
      if (total == kNumLandmarks * 3) landmark_output_idx = static_cast<int>(i);
      else if (total == 1) score_output_idx = static_cast<int>(i);
    }
  }
};

WristEstimator::WristEstimator(const std::string& model_path,
                               float forearm_axis_deg,
                               float neutral_threshold_deg,
                               bool flip_sign)
    : p_(new Impl(model_path, forearm_axis_deg, neutral_threshold_deg, flip_sign)) {}

WristEstimator::~WristEstimator() { delete p_; }

WristResult WristEstimator::Estimate(const uint8_t* image, int width, int height,
                                     int channels, uint64_t frame, uint64_t timestamp) {
  WristResult r{};
  r.frame = frame;
  r.timestamp = timestamp;
  r.class_name = "neutral";

  cv::Mat src;
  if (channels == 1) {
    cv::Mat gray(height, width, CV_8UC1, const_cast<uint8_t*>(image));
    cv::cvtColor(gray, src, cv::COLOR_GRAY2BGR);
  } else if (channels == 3) {
    src = cv::Mat(height, width, CV_8UC3, const_cast<uint8_t*>(image)).clone();
  } else {
    return r;
  }

  cv::Mat resized, rgb, fimg;
  cv::resize(src, resized, cv::Size(kInputSize, kInputSize));
  cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
  rgb.convertTo(fimg, CV_32FC3, 1.0 / 255.0);

  std::vector<float> input_buf;
  input_buf.resize(kInputSize * kInputSize * 3);
  if (p_->nhwc) {
    std::memcpy(input_buf.data(), fimg.data, input_buf.size() * sizeof(float));
  } else {
    std::vector<cv::Mat> ch(3);
    cv::split(fimg, ch);
    for (int c = 0; c < 3; ++c)
      std::memcpy(input_buf.data() + c * kInputSize * kInputSize, ch[c].data,
                  kInputSize * kInputSize * sizeof(float));
  }

  std::vector<int64_t> in_shape = p_->nhwc
      ? std::vector<int64_t>{1, kInputSize, kInputSize, 3}
      : std::vector<int64_t>{1, 3, kInputSize, kInputSize};

  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value in_tensor = Ort::Value::CreateTensor<float>(
      mem, input_buf.data(), input_buf.size(), in_shape.data(), in_shape.size());

  std::vector<const char*> in_names = {p_->input_name.c_str()};
  std::vector<const char*> out_names_c;
  for (auto& n : p_->output_names) out_names_c.push_back(n.c_str());

  auto outputs = p_->session->Run(Ort::RunOptions{nullptr}, in_names.data(), &in_tensor, 1,
                                  out_names_c.data(), out_names_c.size());

  const float* lm = outputs[p_->landmark_output_idx].GetTensorData<float>();
  float score = 1.0f;
  if (p_->score_output_idx >= 0) {
    score = outputs[p_->score_output_idx].GetTensorData<float>()[0];
  }

  // Landmarks come back in input-image (224x224) coordinates. We only need the
  // direction vector wrist -> middle MCP, so the scale doesn't matter.
  float wx = lm[kWristIdx * 3 + 0];
  float wy = lm[kWristIdx * 3 + 1];
  float mx = lm[kMiddleMcpIdx * 3 + 0];
  float my = lm[kMiddleMcpIdx * 3 + 1];

  float dx = mx - wx;
  // image y grows downward; flip so positive angle = upward in image.
  float dy = -(my - wy);
  float hand_deg = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
  float angle = NormalizeAngle180(hand_deg - p_->forearm_axis_deg);
  if (p_->flip_sign) angle = -angle;

  r.valid = true;
  r.angle_deg = angle;
  r.confidence = score;
  if (angle > p_->neutral_threshold_deg) r.class_name = "extensor";
  else if (angle < -p_->neutral_threshold_deg) r.class_name = "flexor";
  else r.class_name = "neutral";
  return r;
}

std::string WristEstimator::ToJson(const WristResult& r) const {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "{\"frame\":%llu,\"timestamp\":%llu,\"valid\":%s,"
                "\"angle_deg\":%.3f,\"class\":\"%s\",\"confidence\":%.3f}",
                static_cast<unsigned long long>(r.frame),
                static_cast<unsigned long long>(r.timestamp),
                r.valid ? "true" : "false",
                r.angle_deg, r.class_name, r.confidence);
  return std::string(buf);
}
