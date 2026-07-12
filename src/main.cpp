#include "utils/utils.h"
#include "FitsReader/FitsReader.h"
#include "SharpnessEstimator/SharpnessEstimator.h"
#include "CameraFinder/CameraFinder.h"
#include <cstdio>
#include <print>
#include <string>
#include <opencv2/core/cvstd.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

cv::Mat readFile(const std::string &file) {
  auto ext = fs::path(file).extension().string();
  if (ext == ".fit" || ext == ".fits") {
    FitsReader fits;
    cv::Mat image = fits.read(file);
    return image;
  } else {
    return cv::imread(file);
  }
}

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::println("OpenCV Focuser Client");

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  // TODO

  //////////////////////////////////////

  const std::string dir = joinArgs(nArgs, args);
  const std::vector files = readDir(dir);

  for (auto &file : files) {
    const cv::Mat image = readFile(file);

    const int w = image.cols;
    const int h = image.rows;
    const auto rect = cv::Rect(w / 4, h / 4, w / 2, h / 2);
    const auto roi = image(rect);

    // cv::Mat preview;
    // cv::resize(image, preview, cv::Size(640, 480));
    // cv::imshow("Image", image);
    // cv::waitKey(0);

    SharpnessEstimator estimator;
    auto sharpness = estimator.getSharpnessGaussian(roi);
    auto name = fs::path(file).filename().string();
    std::println("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}