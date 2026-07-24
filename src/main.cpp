#include "config.h"
#include "indi/INDIClient.h"
#include "SharpnessEstimator.h"
#include "logging/Logger.h"
#include "utils/utils.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <opencv2/highgui.hpp>
#include <print>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::println("{}", rgb("OpenCV Focuser", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  Config config;
  Logger logger(config.logFilePath);
  INDIClient indi(logger);

  logger.info("Connecting to {}:{}...", config.indiHost, config.indiPort);
  auto isConnected = indi.connect(config.indiHost, config.indiPort).get();
  if (!isConnected) {
    return -1;
  }

  logger.info("\nCameras:");
  for (auto &x : indi.getCameras()) {
    logger.info("  * {}: {}x{}", x.name, x.width, x.height);
  }

  logger.info("\nFocusers:");
  for (auto &x : indi.getFocusers()) {
    logger.info("  * {}", x.name);
  }

  return 0;
  //////////////////////////////////////

  std::vector<std::pair<int, double>> results;

  bool isOutward = true;
  const int nIterations = 10;
  for (int i = 0, focus = 0; i < nIterations; ++i) {
    logger.info("\nShooting...");
    auto image = indi.shoot(config.cameraExposure).get();
    auto sharpness = SharpnessEstimator::gaussian(image);
    results.push_back({focus, sharpness});
    logger.info("  Sharpness: {}", sharpness);

    if (i < nIterations - 1) {
      logger.info("\nFocusing...");
      auto direction = i < nIterations / 2 ? isOutward : !isOutward;
      focus = indi.focus(direction, config.focuserStepSize).get();
      std::this_thread::sleep_for(1s); // Wait for the vibration to fade
      logger.info("  Focus: {}", focus);
    } else {
      cv::Mat preview;
      const auto size = cv::Size(640, 480);
      cv::resize(image, preview, size);
      cv::imshow("Exposure", preview);
      cv::waitKey(0);
      cv::destroyAllWindows();
    }
  }

  logger.info("\nFocus / sharpness:");

  const auto sum = 
  std::ranges::fold_left(results, 0.0, [](auto acc, const auto& item) {
    return acc + item.second;
  });

  for (const auto &[index, sharpness] : results) {
    logger.info("  * {:>6}: {}", index, sharpness / sum);
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
    logger.info("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}