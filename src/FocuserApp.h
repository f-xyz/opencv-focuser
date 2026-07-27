#pragma once

#include "Config.h"
#include "indi/INDIClient.h"
#include "logging/Logger.h"
#include "math/SharpnessEstimator.h"
#include "math/Solver.h"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std::chrono_literals;

class FocuserApp final {
  Config &config;
  Logger &logger;
  INDIClient &indi;
  SharpnessEstimator &estimator;
  Solver &solver;

  int focusPosition = 0;
  bool isFocusingOutward = true;

  struct ImageResult {
    cv::Mat image {};
    double sharpness = 0;
    double delta = 0;
  };

public:
  explicit FocuserApp(Config &config, Logger &logger, INDIClient &indi,
    SharpnessEstimator &estimator, Solver &solver) :
    config(config),
    logger(logger),
    indi(indi),
    estimator(estimator),
    solver(solver) {}

  bool connect() {
    bool isConnected = indi.connect(config.indiHost, config.indiPort).get();
    if (isConnected) {
      logger.info("Ready\n");
      reportCameras();
      reportFocusers();
      return true;
    } else {
      return false;
    }
  }

  bool autoFocus() {
    loopAndGatherData();

    auto bestPosition = solver.findBestPosition();
    auto delta = bestPosition - focusPosition;

    logger.info("");
    logger.info("Last position: {}", focusPosition);
    logger.info("Best position: {}", bestPosition);
    logger.info("Delta: {}", delta);

    if (std::abs(delta) > config.focuserStepSize * 5) {
      logger.error("The ideal focus position if too far.");
      return false;
    }

    move(delta > 0, std::abs(delta));
    
    auto result = shoot(config.cameraExposure);
    preview(result.image);

    return true;
  }

  //////////////////////////////////////
  //////////////////////////////////////
  //////////////////////////////////////

private:
  void reportCameras() {
    logger.info("Cameras:");
    const auto cameras = indi.getCameras();
    for (int i = 0; i < cameras.size(); ++i) {
      auto marker = i == 0 ? '>' : '*';
      auto camera = cameras[i];
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
      auto focuser = focusers[i];
      logger.info("  {} {}: {}", marker, focuser.name, focuser.position);
    }
    logger.info("");
  }

  bool loopAndGatherData() {
    for (int i = 0; i < config.nIterations; ++i) {
      logger.info("Iteration #{} of {}", i + 1, config.nIterations);
      auto result = shoot(config.cameraExposure);

      // Skip focusing at the last iteration
      if (i < config.nIterations - 1) {
        // Swap direction if needed
        if (result.delta < 0) {
          isFocusingOutward = !isFocusingOutward;
        }
        move(isFocusingOutward, config.focuserStepSize);
      }
    }

    return true;
  }

  ImageResult shoot(double exposure) {
    auto image = indi.image(exposure).get();
    auto roi = getROI(image);
    auto sharpness = estimator.getSharpness(image);
    auto delta = solver.addPoint(focusPosition, sharpness);

    logger.info("Sharpness: {}", sharpness);
    logger.info("Delta: {}", delta);

    if (sharpness == 0) {
      logger.error("Invalid image: either all white or all black.");
      preview(image);
      return {};
    }

    return {
      image,
      sharpness,
      delta
    };
  }

  static cv::Mat getROI(const cv::Mat &image) {
    const int w = image.cols;
    const int h = image.rows;
    const auto rect = cv::Rect(w / 4, h / 4, w / 2, h / 2);
    const auto roi = image(rect);

    return roi;
  }

  int move(const bool isOutward, const unsigned int steps) {
    if (isOutward) {
      focusPosition = indi.focus(true, steps).get();
      logger.info("Position: {}\n", focusPosition);
    } else {
      // Move inward
      const auto inwardSteps = steps + config.focuserBacklash;
      focusPosition = indi.focus(false, inwardSteps).get();
      // And move outward a bit
      focusPosition = indi.focus(true, config.focuserBacklash).get();
      logger.info("Position: {}\n", focusPosition);
    }
    
    // Wait for the vibration to fade
    std::this_thread::sleep_for(1s);

    return focusPosition;
  }

  static void preview(const cv::Mat &image) {
    cv::Mat flipped, preview;
    cv::flip(image, flipped, 1); // Flip horizontally
    cv::resize(flipped, preview, cv::Size(640, 480));
    cv::imshow("Preview", preview);
    cv::waitKey(0);
    cv::destroyAllWindows();
  }
};