#include "config.h"
#include "indi/INDIClient.h"
#include "SharpnessEstimator.h"
#include "logging/Logger.h"
#include "utils/utils.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <future>
#include <opencv2/highgui.hpp>
#include <print>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class FocuserApp final {
  Config &config;
  Logger &logger;
  INDIClient &indi;
  std::promise<bool> startPromise;
  std::vector<std::pair<int, double>> results;

public:
  FocuserApp(Config &config, Logger &logger, INDIClient &indi)
      : config(config), logger(logger), indi(indi) {}

  std::future<bool> connect() {
    auto future = startPromise.get_future();

    auto isConnected = indi.connect(config.indiHost, config.indiPort).get();
    startPromise.set_value(isConnected);

    if (isConnected) {
      logger.info("Ready\n");
    }

    return future;
  }

  void reportCameras() {
    logger.info("Cameras:");
    const auto cameras = indi.getCameras();
    for (int i = 0; i < cameras.size(); ++i) {
      auto marker = i == 0 ? '>' : '*';
      auto &camera = cameras[i];
      logger.info("  {} {}: {}x{}", marker,
        camera.name, camera.width, camera.height);
    }
    logger.info("");
  }

  void reportFocusers() {
    logger.info("Focusers:");
    const auto focusers = indi.getFocusers();
    for (int i = 0; i < focusers.size(); ++i) {
      auto marker = i == 0 ? '>' : '*';
      auto &focuser = focusers[i];
      logger.info("  {} {}: {}", marker, focuser.name, focuser.position);
    }
    logger.info("");
  }
};

struct FocusPoint {
  int position = 0;
  double sharpness = 0;
};

class Results {
  Logger &logger;
  std::map<int, std::vector<double>> map;

public:
  Results(Logger &logger) : logger(logger) {}

  void addPoint(int position, double sharpness) {
    map[position].push_back(sharpness);
  }

  void report() {
    logger.info("\nFocus / sharpness:");

    for (auto [position, sharpnesses] : map) {
      const auto size = sharpnesses.size();
      const auto sum = std::ranges::fold_left(sharpnesses,
         0.0, 
         [](auto res, const auto& x) { return res + x; });
      const auto average = sum / size;

      logger.info("{:<6}: {:.4f} ({})",
        position >= 0
          ? "+" + std::to_string(std::abs(position))
          : "-" + std::to_string(std::abs(position)),
        average,
        size);
    }
  }
};

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::println("{}", rgb("OpenCV Focuser v0.0.1\n", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  Config config;
  Logger logger(config.logFilePath);
  INDIClient indi(logger);
  Results results(logger);
  FocuserApp app(config, logger, indi);

  app.connect().get();
  app.reportCameras();
  app.reportFocusers();

  double lastSharpness = 0;
  int lastPosition = 0;
  bool isOutward = true;

  const int nIterations = 10;
  for (int i = 0; i < nIterations; ++i) {
    auto image = indi.shoot(config.cameraExposure).get();
    auto sharpness = SharpnessEstimator::gaussian(image);
    auto delta = lastSharpness ? sharpness - lastSharpness : 0;
    results.addPoint(lastPosition, sharpness);
    lastSharpness = sharpness;
    logger.info("Sharpness: {}", sharpness);
    logger.info("Delta: {}", delta);

    if (i < nIterations - 1) {
      if (delta < 0) {
        isOutward = !isOutward;
      }

      auto position = indi.focus(isOutward, config.focuserStepSize).get();
      lastPosition = position;
      std::this_thread::sleep_for(1s); // Wait for the vibration to fade
      logger.info("Position: {}\n", position);
    } else {
      const cv::Size size(640, 480);
      cv::Mat preview;
      cv::resize(image, preview, size);
      cv::imshow("Exposure", preview);
      cv::waitKey(0);
      cv::destroyAllWindows();
    }
  }

  results.report();

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