#include "config.h"
#include "indi/INDIClient.h"
#include "SharpnessEstimator.h"
#include "utils/colors.h"
#include "utils/utils.h"
#include <algorithm>
#include <cstdlib>
#include <opencv2/highgui.hpp>
#include <print>
#include <thread>
#include <unistd.h>
#include <vector>

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
  bool isOK = indi.connect(config.indiHost, config.indiPort).get();
  std::println("\nInitialized: {}", isOK);

  std::println("\nCameras:");
  for (auto &x : indi.getCameras()) {
    std::println("  * {}: {}x{}", x.name, x.width, x.height);
  }

  std::println("\nFocusers:");
  for (auto &x : indi.getFocusers()) {
    std::println("  * {}", x.name);
  }

  return 0; /////////////////////////////////////////////

  std::vector<std::pair<int, double>> results;

  bool isOutward = true;
  const int nIterations = 10;
  for (int i = 0, focus = 0; i < nIterations; ++i) {
    std::println("\nShooting...");
    auto image = indi.shoot(0.02).get();
    auto sharpness = SharpnessEstimator::gaussian(image);
    results.push_back({focus, sharpness});
    std::println("  * Sharpness: {}", sharpness);

    if (i < nIterations - 1) {
      std::println("\nFocusing...");
      int increment = 500;
      auto motion = indi.move(isOutward, increment).get();
      focus += isOutward ? increment : -increment;
      // Wait for the vibration to fade
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1s);
      std::println("Moved: {}", motion);
    } else {
      cv::Mat preview;
      const auto size = cv::Size(640, 480);
      cv::resize(image, preview, size);
      cv::imshow("Exposure", preview);
      cv::waitKey(0);
      cv::destroyAllWindows();
    }
  }

  const auto sum = 
  std::ranges::fold_left(results, 0.0, [](auto acc, const auto& item) {
    return acc + item.second;
  });

  std::println("Focus / sharpness:");
  for (const auto &[index, sharpness] : results) {
    std::println("  * {:>6}: {}", index, sharpness / sum);
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