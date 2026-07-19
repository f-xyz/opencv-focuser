#include "FitsReader/FitsReader.h"
#include "INDIClient/INDIClient.h"
#include "SharpnessEstimator/SharpnessEstimator.h"
#include "utils/colors.h"
#include "utils/utils.h"
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

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
  std::println("{}", rgb("OpenCV Focuser", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  INDIClient indi([&indi]() {
    std::println("\nCameras:");
    for (auto &x : indi.cameras) {
      std::println("  * {}: {}x{}", x.name, x.width, x.height);
    }

    std::println("\nShooting...");
    auto camera = indi.cameras.front();
    auto result = indi.shoot(camera, 0.1);
    auto image = result.get();

    cv::imshow("Exposure", image);
    cv::waitKey(0);
    cv::destroyAllWindows();
    exit(0);
  });

  std::cin.get();
  return 0;

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