#pragma once

#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>

class SharpnessEstimator {
public:
  static double gaussian(const cv::Mat &image) {
    constexpr double sigmaNarrow = 1;
    constexpr double sigmaWide = 10;

    cv::Mat narrow, wide, difference;

    const auto kernel = cv::Size(0, 0);
    cv::GaussianBlur(image, narrow, kernel, sigmaNarrow, sigmaNarrow);
    cv::GaussianBlur(image, wide, kernel, sigmaWide, sigmaWide);
    cv::subtract(narrow, wide, difference);

    cv::Scalar mean, stdDev;
    cv::meanStdDev(difference, mean, stdDev);

    return stdDev[0];
  }

  static double laplacian(const cv::Mat &image) {
    cv::Mat blurred, laplacian;

    const auto kernel = cv::Size(0, 0);
    cv::GaussianBlur(image, blurred, kernel, 3);
    cv::Laplacian(blurred, laplacian, CV_64F);

    cv::Scalar mean, stdDev;
    cv::meanStdDev(laplacian, mean, stdDev);

    return stdDev[0];
  }
};