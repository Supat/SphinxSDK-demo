#include "pose.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr int kInputSize = 256;
constexpr int kStride = 5;            // x, y, z, visibility, presence
constexpr int kLandmarkOutputLen = 39 * kStride;
constexpr int kLeftElbow = 13;
constexpr int kRightElbow = 14;
constexpr int kLeftWrist = 15;
constexpr int kRightWrist = 16;

float Sigmoid(float x) {
  // raw values can already be in [0,1] for some exports; sigmoid(>>1) saturates so this is safe.
  return 1.0f / (1.0f + std::exp(-x));
}

}  // namespace

struct PoseEstimator::Impl {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "pose"};
  Ort::SessionOptions opts;
  std::unique_ptr<Ort::Session> session;
  Ort::AllocatorWithDefaultOptions alloc;

  std::string input_name;
  std::vector<std::string> output_names;
  bool nhwc = true;
  int landmark_output_idx = 0;

  float min_visibility;

  Impl(const std::string& model_path, float min_vis) : min_visibility(min_vis) {
    opts.SetIntraOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    std::wstring wpath(model_path.begin(), model_path.end());
    session = std::make_unique<Ort::Session>(env, wpath.c_str(), opts);

    auto in_name = session->GetInputNameAllocated(0, alloc);
    input_name = in_name.get();

    auto in_info = session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
    auto in_shape = in_info.GetShape();
    if (in_shape.size() == 4 && in_shape[1] == 3) nhwc = false;

    size_t n_out = session->GetOutputCount();
    bool got = false;
    for (size_t i = 0; i < n_out; ++i) {
      auto n = session->GetOutputNameAllocated(i, alloc);
      output_names.emplace_back(n.get());
      if (got) continue;
      auto info = session->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo();
      auto shp = info.GetShape();
      int64_t total = 1;
      for (auto d : shp) total *= (d > 0 ? d : 1);
      if (total == kLandmarkOutputLen) {
        landmark_output_idx = static_cast<int>(i);
        got = true;
      }
    }
  }
};

PoseEstimator::PoseEstimator(const std::string& model_path, float min_visibility)
    : p_(new Impl(model_path, min_visibility)) {}

PoseEstimator::~PoseEstimator() { delete p_; }

ForearmResult PoseEstimator::Estimate(const uint8_t* image, int width, int height,
                                      int channels) {
  ForearmResult fr{};
  fr.source = "none";

  cv::Mat src;
  if (channels == 1) {
    cv::Mat gray(height, width, CV_8UC1, const_cast<uint8_t*>(image));
    cv::cvtColor(gray, src, cv::COLOR_GRAY2BGR);
  } else if (channels == 3) {
    src = cv::Mat(height, width, CV_8UC3, const_cast<uint8_t*>(image)).clone();
  } else {
    return fr;
  }

  cv::Mat resized, rgb, fimg;
  cv::resize(src, resized, cv::Size(kInputSize, kInputSize));
  cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
  rgb.convertTo(fimg, CV_32FC3, 1.0 / 255.0);

  std::vector<float> input_buf(kInputSize * kInputSize * 3);
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

  auto vis = [&](int idx) { return Sigmoid(lm[idx * kStride + 3]); };
  auto x = [&](int idx) { return lm[idx * kStride + 0]; };
  auto y = [&](int idx) { return lm[idx * kStride + 1]; };

  float left_conf = std::min(vis(kLeftElbow), vis(kLeftWrist));
  float right_conf = std::min(vis(kRightElbow), vis(kRightWrist));

  int e_idx = -1, w_idx = -1;
  float chosen_conf = 0.0f;
  const char* src_name = "none";
  if (left_conf >= right_conf && left_conf >= p_->min_visibility) {
    e_idx = kLeftElbow; w_idx = kLeftWrist;
    chosen_conf = left_conf;
    src_name = "left_forearm";
  } else if (right_conf > left_conf && right_conf >= p_->min_visibility) {
    e_idx = kRightElbow; w_idx = kRightWrist;
    chosen_conf = right_conf;
    src_name = "right_forearm";
  } else {
    return fr;  // valid stays false
  }

  // Landmarks come in input-pixel space [0, kInputSize]; rescale to source image.
  const float sx = static_cast<float>(width) / kInputSize;
  const float sy = static_cast<float>(height) / kInputSize;
  float ex = x(e_idx) * sx, ey = y(e_idx) * sy;
  float wx = x(w_idx) * sx, wy = y(w_idx) * sy;

  float dx = wx - ex;
  float dy = -(wy - ey);  // image y grows down; flip so positive = up
  float deg = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;

  fr.valid = true;
  fr.angle_deg = deg;
  fr.source = src_name;
  fr.confidence = chosen_conf;
  fr.elbow_x = ex; fr.elbow_y = ey;
  fr.wrist_x = wx; fr.wrist_y = wy;
  return fr;
}
