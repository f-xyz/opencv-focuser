#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

class SharpnessEstimator {
public:
  virtual ~SharpnessEstimator() = default;
  virtual double getSharpness(const cv::Mat &image) const = 0;

  static cv::Mat getGrayscaleImage(const cv::Mat &image) {
    cv::Mat gray, result;

    // Make grayscale
    if (image.channels() == 3) {
      cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
      gray = image;
    }

    // Convert to float
    gray.convertTo(result, CV_32F);

    return result;
  }

  static cv::Mat getBlurredImage(const cv::Mat &image, double sigma) {
    if (sigma > 0.0) {
      cv::Mat result;
      cv::Size kernel(0, 0);
      cv::GaussianBlur(image, result, kernel, sigma, sigma);
      return result;
    } else {
      return image;
    }
  }

  static double getStdDev(const cv::Mat &image) {
    cv::Scalar mean, stdDev;
    cv::meanStdDev(image, mean, stdDev);
    return stdDev[0];
  }
};

class SharpnessEstimatorGaussian final : public SharpnessEstimator {
  double sigmaHigh = 1;
  double sigmaLow = 10;

public:
  SharpnessEstimatorGaussian() = default;
  SharpnessEstimatorGaussian(double sigmaNarrow, double sigmaWide)
    : sigmaHigh(sigmaNarrow), sigmaLow(sigmaWide) {}

  double getSharpness(const cv::Mat &image) const override {
    cv::Mat gray = getGrayscaleImage(image);

    cv::Mat high = getBlurredImage(gray, sigmaHigh);
    cv::Mat low = getBlurredImage(gray, sigmaLow);

    cv::Mat difference;
    cv::subtract(high, low, difference);

    return getStdDev(difference);
  }
};

class SharpnessEstimatorLaplacian final : public SharpnessEstimator {
  double sigmaHigh = 0;

public:
  SharpnessEstimatorLaplacian() = default;
  SharpnessEstimatorLaplacian(double sigma)
    : sigmaHigh(sigma) {}

  double getSharpness(const cv::Mat &image) const override {
    cv::Mat gray = getGrayscaleImage(image);
    cv::Mat blurred = getBlurredImage(gray, sigmaHigh);

    cv::Mat laplacian;
    cv::Laplacian(blurred, laplacian, CV_32F);

    return getStdDev(laplacian);
  }
};