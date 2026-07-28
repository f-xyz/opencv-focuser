#pragma once

#include "Config.h"
#include "indi/INDIClient.h"
#include "logging/Logger.h"
#include "math/SharpnessEstimator.h"
#include "math/Solver.h"
#include "utils/utils.h"
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
    ////////////////////////////////////
    // Gather data points //////////////
    ////////////////////////////////////

    for (int i = 0; i < config.nIterations; ++i) {
      logger.info("Iteration #{} of {}", i + 1, config.nIterations);
      auto result = shoot(config.cameraExposure);

      // Skip focusing at the last iteration
      if (i >= config.nIterations - 1) {
        continue;
      }

      // Swap direction if needed
      if (result.delta < -1) { // Just above camera noise
        isFocusingOutward = !isFocusingOutward;
      }

      move(isFocusingOutward, config.focuserStepSize);
    }

    ////////////////////////////////////
    // Solve ///////////////////////////
    ////////////////////////////////////

    auto solution = solver.findBestPosition();

    switch (solution.type) {
      case MoveInward:
        logger.info(">>> {}", rgb("MoveInvard", 0xFFFFFF));
        autoFocus();
        break;
      case MoveOutward:
        logger.info(">>> {}", rgb("MoveOutward", 0xFFFFFF));
        autoFocus();
        break;
      case MoveAround:
        logger.info(">>> {}", rgb("MoveAround", 0xFFFFFF));
        validateSolution(solution);
        break;
      }

    return true;
  }

  bool validateSolution(const Solution &solution) {
    auto bestPoint = solution.point;
    auto bestPosition = bestPoint.position;
    auto stepsToBest = bestPosition - focusPosition;

    logger.info("Last position: {}", focusPosition);
    logger.info("Best position: {}", bestPosition);
    logger.info("Steps to the best position: {}", formatNumber(stepsToBest));

    // Prevent equipment damage...
    if (std::abs(stepsToBest) > config.focuserStepSize * config.nIterations) {
      logger.error("The ideal focus position if too far.");
      return false;
    }

    auto isOutward = stepsToBest > 0;
    auto steps = std::abs(stepsToBest);
    move(isOutward, steps);

    auto result = shoot(config.cameraExposure);
    auto delta = result.sharpness - bestPoint.sharpness;
    preview(result.image);

    return true;
  }

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

  ImageResult shoot(double exposure) {
    cv::Mat image;
    std::vector<double> sharpnesses;

    for (int i = 0; i < config.cameraAverageFrames; ++i) {
      image = indi.image(exposure).get();

      auto roi = getROI(image);
      auto sharpness = estimator.getSharpness(image);
      sharpnesses.push_back(sharpness);
    }

    auto sum = std::ranges::fold_left( sharpnesses, 0.0, std::plus {});
    auto sharpness = sum / sharpnesses.size();
    auto delta = solver.addPoint(focusPosition, sharpness);

    logger.info("Sharpness: {}; Delta: {}",
      formatNumber(sharpness),
      formatNumber(delta));

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
      logger.info("Position: {}\n", formatNumber(focusPosition));
    } else {
      // Move inward
      const auto inwardSteps = steps + config.focuserBacklash;
      focusPosition = indi.focus(false, inwardSteps).get();
      // And move outward a bit
      focusPosition = indi.focus(true, config.focuserBacklash).get();
      logger.info("Position: {}\n", formatNumber(focusPosition));
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