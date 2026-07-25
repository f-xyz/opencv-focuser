#include "config.h"
#include "indi/INDIClient.h"
#include "math/SharpnessEstimator.h"
#include "logging/Logger.h"
#include "utils/utils.h"
#include "math/Solver.h"
#include <cmath>
#include <cstdlib>
#include <future>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class FocuserApp final {
  Config &config;
  Logger &logger;
  INDIClient &indi;
  std::promise<bool> startPromise;

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

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  // std::signal(SIGSEGV, onSegfault);
  std::println("{}", rgb("OpenCV Focuser v0.0.1\n", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  Config config;
  Logger logger(config.logFilePath);
  INDIClient indi(logger);
  Solver solver(config, logger);
  FocuserApp app(config, logger, indi);

  app.connect().get();
  app.reportCameras();
  app.reportFocusers();

  bool isOutward = true;
  int lastPosition = 0;

  for (int i = 0; i < config.nIterations; ++i) {
    logger.info("Iteration #{} of {}", i + 1, config.nIterations);

    auto image = indi.shoot(config.cameraExposure).get();
    auto sharpness = SharpnessEstimator::laplacian(image);
    auto delta = solver.addPoint(lastPosition, sharpness);

    logger.info("Sharpness: {}", sharpness);
    logger.info("Delta: {}", delta);

    if (!sharpness) {
      logger.error("Invalid image: either all white or all black.");

      const cv::Size size(640, 480);
      cv::Mat preview;
      cv::resize(image, preview, size);
      cv::imshow("Exposure", preview);
      cv::waitKey(0);
      cv::destroyAllWindows();
      return 1;
    }

    // Skip focusing at the last iteration
    if (i < config.nIterations - 1) {
      // Swap direction if needed
      if (delta < -sharpness / 100) {
        isOutward = !isOutward;
      }

      auto position = indi.focus(isOutward, config.focuserStepSize).get();
      lastPosition = position;
      std::this_thread::sleep_for(1s); // Wait for the vibration to fade
      logger.info("Position: {}\n", position);
    }
  }

  int bestPosition = solver.findBestPosition();
  auto delta = bestPosition - lastPosition;
  logger.info("");
  logger.info("Last position: {}", lastPosition);
  logger.info("Best position: {}", bestPosition);
  logger.info("Delta: {}", delta);

  if (std::abs(delta) > config.focuserStepSize * 3) {
    logger.error("The ideal focus position if too far.");
    return 2;
  }

  auto position = indi.focus(delta > 0, std::abs(delta)).get();
  std::this_thread::sleep_for(1s); // Wait for the vibration to fade
  logger.info("Position: {}\n", position);

  auto image = indi.shoot(config.cameraExposure).get();
  auto sharpness = SharpnessEstimator::laplacian(image);
  logger.info("Sharpness: {}", sharpness);

  cv::Mat preview;
  cv::resize(image, preview, cv::Size(640, 480));
  cv::imshow("Exposure", preview);
  cv::waitKey(0);
  cv::destroyAllWindows();

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