#include "FocuserApp.h"

bool FocuserApp::connect() {
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

bool FocuserApp::autoFocus() {
  gatherDataByHeart();
  // gatherDataLinearly();

  auto solution = solver.findBestPosition();
  solver.reset();

  checkSolution(solution);

  return true;
}

bool FocuserApp::gatherDataByHeart() {
  for (int i = 0; i < config.nIterations; ++i) {
    logger.info("Iteration #{} of {}", i + 1, config.nIterations);

    auto result = shoot(config.cameraExposure);

    logger.info("Sharpness: {}; Delta: {}",
      formatNumber(result.sharpness),
      formatNumber(result.delta));

    if (result.sharpness == 0) {
      logger.error("Invalid image: either all white or all black.");
      preview(result.image);
      return false;
    }

    // Skip focusing at the last iteration
    if (i >= config.nIterations - 1) {
      continue;
    }

    // Swap direction if needed
    if (result.delta < -1) { // Just above camera noise
      isFocusingOutward = !isFocusingOutward;
    }

    move(isFocusingOutward, config.focuserStepSize);
    logger.info("Position: {}\n", formatNumber(focusPosition));
  }

  return true;
}

bool FocuserApp::gatherDataLinearly() {
  // Move 2 steps inward
  move(false, config.focuserStepSize * 2);
  logger.info("Position: {}\n", formatNumber(focusPosition));

  auto result = shoot(config.cameraExposure);
  logger.info("Sharpness: {}; Delta: {}",
      formatNumber(result.sharpness),
      formatNumber(result.delta));

  // Move 1 step outward 4 times
  for (int i = 0; i < 4; ++i) {
    logger.info("\nIteration #{} of {}", i + 1, config.nIterations);

    // Move 1 step outward
    move(true, config.focuserStepSize * 2);
    logger.info("Position: {}\n", formatNumber(focusPosition));

    // Capture an image
    auto result = shoot(config.cameraExposure);
    logger.info("Sharpness: {}; Delta: {}",
      formatNumber(result.sharpness),
      formatNumber(result.delta));

    if (result.sharpness == 0) {
      logger.error("Invalid image: either all white or all black.");
      preview(result.image);
      return false;
    }
  }

  // Return to the initial position
  move(false, config.focuserStepSize * 2);

  return true;
}

bool FocuserApp::checkSolution(const Solution &solution) {
  logger.info("The ideal position is:");
  switch (solution.type) {
    case Inward:
      logger.info("  {}\n", rgb("Inward", 0xFFFFFF));
      // logger.info("Press any key to continue...");
      // std::cin.get();
      // autoFocus();
      break;

    case Outward:
      logger.info("  {}\n", rgb("Outward", 0xFFFFFF));
      // logger.info("Press any key to continue...");
      // std::cin.get();
      // autoFocus();
      break;

    case Around:
      logger.info("  {}\n", rgb("Around", 0xFFFFFF));

      if (validateSolution(solution)) {
        // logger.info("{}", rgb("Done!", 0xFFFFFF));
      } else {
        // logger.info("Press any key to continue...");
        // std::cin.get();
        // config.focuserStepSize /= 5;
        // autoFocus();
      }
      break;
  }

  return true;
}

bool FocuserApp::validateSolution(const Solution &solution) {
  auto bestPoint = solution.bestPoint;
  auto idealPoint = solution.idealPoint;
  auto idealPosition = idealPoint.position;
  auto stepsToIdeal = idealPosition - focusPosition;

  logger.info("Best sharpnes: {}", bestPoint.sharpness);
  logger.info("Ideal sharpnes: {}", idealPoint.sharpness);
  logger.info("");

  logger.info("Current position: {}", focusPosition);
  logger.info("Ideal position: {}", idealPosition);
  logger.info("Steps to the best position: {}", formatNumber(stepsToIdeal));
  logger.info("");

  // Prevent equipment damage...
  if (std::abs(stepsToIdeal) > config.focuserStepSize * config.nIterations) {
    logger.error("The ideal focus position if too far.");
    return false;
  }

  // Move to the ideal position
  auto isOutward = stepsToIdeal > 0;
  auto steps = std::abs(stepsToIdeal);
  move(isOutward, steps);

  // Take an image and compare it with the best
  auto result = shoot(config.cameraExposure);
  auto delta = result.sharpness - bestPoint.sharpness;

  logger.info("Sharpness: {}; Delta: {}",
    formatNumber(result.sharpness),
    formatNumber(result.delta));

  preview(result.image);

  return delta >= 0;
}

void FocuserApp::reportCameras() {
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

void FocuserApp::reportFocusers() {
  logger.info("Focusers:");
  const auto focusers = indi.getFocusers();
  for (int i = 0; i < focusers.size(); ++i) {
    auto marker = i == 0 ? '>' : '*';
    auto focuser = focusers[i];
    logger.info("  {} {}: {}", marker, focuser.name, focuser.position);
  }
  logger.info("");
}

FocuserApp::ImageResult FocuserApp::shoot(double exposure) {
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

  return {
    image,
    sharpness,
    delta
  };
}

cv::Mat FocuserApp::getROI(const cv::Mat &image) {
  const int w = image.cols;
  const int h = image.rows;
  const auto rect = cv::Rect(w / 4, h / 4, w / 2, h / 2);
  const auto roi = image(rect);

  return roi;
}

int FocuserApp::move(const bool isOutward, const unsigned int steps) {
  if (isOutward) {
    focusPosition = indi.focus(true, steps).get();
  } else {
    // Move inward
    const auto inwardSteps = steps + config.focuserBacklash;
    focusPosition = indi.focus(false, inwardSteps).get();
    // And move outward a bit after a moment
    std::this_thread::sleep_for(100ms);
    focusPosition = indi.focus(true, config.focuserBacklash).get();
  }

  // Wait for the vibration to fade
  std::this_thread::sleep_for(1s);

  return focusPosition;
}

  void FocuserApp::preview(const cv::Mat &image) {
  cv::Mat flipped, preview;
  cv::flip(image, flipped, 1); // Flip horizontally
  cv::resize(flipped, preview, cv::Size(640, 480));
  cv::imshow("Preview", preview);
  cv::waitKey(0);
  cv::destroyAllWindows();
}