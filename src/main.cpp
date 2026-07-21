#include "config.h"
#include "indi/INDIClient.h"
#include "SharpnessEstimator.h"
#include "utils/colors.h"
#include "utils/utils.h"
#include <opencv2/highgui.hpp>
#include <print>

namespace fs = std::filesystem;

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::println("{}", rgb("OpenCV Focuser", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  Config config;
  INDIClient indi;

  std::println("Connecting to {}:{}", config.indiHost, config.indiPort);
  indi.connect(config.indiHost, config.indiPort).get();

  std::println("\nCameras:");
  for (auto &x : indi.getCameras()) {
    std::println("  * {}: {}x{}", x.name, x.width, x.height);
  }

  std::println("\nFocusers:");
  for (auto &x : indi.getFocusers()) {
    std::println("  * {}", x.name);
  }

  for (int i = 0; i < 3; ++i) {
    std::println("\nShooting...");
    auto image = indi.shoot(0.05).get();
    auto sharpness = SharpnessEstimator::gaussian(image);
    std::println("  * Sharpness: {}", sharpness);

    cv::Mat preview;
    const auto size = cv::Size(640, 480);
    cv::resize(image, preview, size);
    cv::imshow("Exposure", preview);
    cv::waitKey(0);
    cv::destroyAllWindows();

    std::println("\nFocusing...");
    auto motion = indi.move(false, 500).get();
    std::println("Moved: {}", motion);
  }

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

    auto sharpness = SharpnessEstimator::gaussian(roi);
    auto name = fs::path(file).filename().string();
    std::println("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}