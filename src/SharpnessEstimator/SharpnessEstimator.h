#pragma once

#include <opencv2/core/mat.hpp>

class SharpnessEstimator {
  public:
    double getSharpnessGaussian(const cv::Mat &image);
    double getSharpnessLaplacian(const cv::Mat &image);
};