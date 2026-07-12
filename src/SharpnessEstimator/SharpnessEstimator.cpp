#include <opencv2/core.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include "SharpnessEstimator.h"

double SharpnessEstimator::getSharpnessGaussian(const cv::Mat &image) {
  constexpr double sigmaNarrow = 1;
  constexpr double sigmaWide = 10;

  cv::Mat narrow, wide, difference;
  cv::Size kernel = cv::Size(0, 0);
  cv::GaussianBlur(image, narrow, kernel, sigmaNarrow, sigmaNarrow);
  cv::GaussianBlur(image, wide, kernel, sigmaWide, sigmaWide);
  cv::subtract(narrow, wide, difference);

  cv::Scalar mean, stdDev;
  cv::meanStdDev(difference, mean, stdDev);

  return stdDev[0];
}

double SharpnessEstimator::getSharpnessLaplacian(const cv::Mat &image) {
  cv::Mat blurred, laplacian;
  cv::Size kernel = cv::Size(0, 0);
  cv::GaussianBlur(image, blurred, kernel, 3);
  cv::Laplacian(blurred, laplacian, CV_64F);

  cv::Scalar mean, stdDev;
  cv::meanStdDev(laplacian, mean, stdDev);

  return stdDev[0];
}