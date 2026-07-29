#include "FocuserApp.h"
#include "utils/colors.h"
#include <exception>

bool FocuserApp::connect() {
  logger.header("Connecting...");
  bool isConnected = indi.connect(config.indiHost, config.indiPort).get();
  if (isConnected) {
    logger.header("\nConnected\n");
    reportCameras();
    reportFocusers();
    return true;
  } else {
    return false;
  }
}

////////////////////////////////////////
// Focusing Routines ///////////////////
////////////////////////////////////////

bool FocuserApp::autoFocus(Type type, bool startOutward) {
  logger.header("Starting auto focus...");
  logger.info("  Type: {}", typeToString(type));
  logger.info("  Direction: {}\n", startOutward ? "outward": "inward");

  // Gather data points
  switch (type) {
    case ByEar:
      gatherDataByEar(startOutward);
      break;

    case Linear:
      gatherDataLinearly();
      break;
  }

  // Find solution
  logger.header("Solving...");
  auto solution = solver.findBestPosition();
  solver.clear();

  // Decide what to do next
  return checkSolution(solution);
}

bool FocuserApp::checkSolution(const Solution &solution) {
  logger.header("Checking the solution...");

  switch (solution.type) {
    case Inward: {
      logger.info("  Ideal position is: {}\n", bold("inward"));

      auto delta = solution.bestPoint.position - focusPosition;
      if (delta != 0) {
        logger.info("Moving to be first position...");
        move(false, std::abs(delta));
      }
      
      logger.info("");
      autoFocus(Type::ByEar, false);
      break;
    }

    case Outward: {
      logger.info("  Ideal position is: {}\n", bold("outward"));
      autoFocus(Type::ByEar, true);
      break;
    }

    case Around: {
      logger.info("  Ideal position is: {}\n", bold("around"));

      if (validateSolution(solution)) {
        logger.header("Done!");
      } else {
        // Not sharp enough! Trying again with smaller steps...
        config.focuserStepSize /= 2;

        if (config.focuserStepSize > 0) {
          autoFocus(Type::Linear, false);
        } else {
          logger.error("I did my best, but failed anyway...");
          return false;
        }
      }
      break;
    }
  }

  return true;
}

bool FocuserApp::validateSolution(const Solution &solution) {
  logger.header("Validating the solution...");

  auto bestPoint = solution.bestPoint;
  auto idealPoint = solution.idealPoint;
  auto idealPosition = idealPoint.position;
  auto stepsToIdeal = idealPosition - focusPosition;

  logger.info("  Best sharpnes: {}", bestPoint.sharpness);
  logger.info("  Ideal sharpnes: {}", idealPoint.sharpness);
  logger.info("");
  
  logger.info("  Current position: {}", focusPosition);
  logger.info("  Ideal position: {}", idealPosition);
  logger.info("  Steps to the best position: {}", formatNumber(stepsToIdeal));
  logger.info("");

  // Prevent equipment damage...
  if (std::abs(stepsToIdeal) > config.focuserStepSize * config.nIterations) {
    logger.error("  The ideal focus position if too far.\n");
    return false;
  }

  // Move to the ideal position
  auto isOutward = stepsToIdeal > 0;
  auto steps = std::abs(stepsToIdeal);
  move(isOutward, steps);
  logger.info("");

  // Capture an image and compare it with the best
  solver.addPoint(bestPoint.position, bestPoint.sharpness);
  auto result = shoot(config.cameraExposure);
  preview(result.image);
  logger.info("");

  return result.delta > 0; // Is not worse than the best?
}

////////////////////////////////////////
// Data Gathering //////////////////////
////////////////////////////////////////

void FocuserApp::gatherDataByEar(bool startOutward) {
  bool isFocusingOutward = startOutward;

  for (int i = 0; i < config.nIterations; ++i) {
    logger.info("Iteration #{} of {} (ByEar)", i + 1, config.nIterations);

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
    logger.info("");
  }
}

void FocuserApp::gatherDataLinearly() {
  // Move 2 steps inward
  move(false, config.focuserStepSize * 2);
  auto result = shoot(config.cameraExposure);

  // Move 1 step outward 4 times
  for (int i = 0; i < 4; ++i) {
    logger.info("\nIteration #{} of {} (Linear)", i + 1, config.nIterations);
    move(true, config.focuserStepSize * 2);
    auto result = shoot(config.cameraExposure);
  }
}

////////////////////////////////////////
// Reporting ///////////////////////////
////////////////////////////////////////

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

////////////////////////////////////////
// INDI wrappers ///////////////////////
////////////////////////////////////////

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

  logger.info("Sharpness: {}; Delta: {}",
    formatNumber(sharpness),
    formatNumber(delta));

  if (sharpness == 0) {
    logger.error("Invalid image: either all white or all black.");
    preview(image);
    std::terminate();
  }

  return {
    image,
    sharpness,
    delta
  };
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

  logger.info("Position: {}", formatNumber(focusPosition));

  return focusPosition;
}

////////////////////////////////////////
// Helpers /////////////////////////////
////////////////////////////////////////

cv::Mat FocuserApp::getROI(const cv::Mat &image) {
  const int w = image.cols;
  const int h = image.rows;

  const auto rect = cv::Rect(
    w / 4,
    h / 4,
    w / 2,
    h / 2);

  return image(rect);
}

void FocuserApp::preview(const cv::Mat &image) {
  cv::Mat flipped, preview;
  cv::flip(image, flipped, 1); // Flip horizontally
  cv::resize(flipped, preview, cv::Size(640, 480));
  cv::imshow("Preview", preview);
  cv::waitKey(0);
  cv::destroyAllWindows();
}

std::string FocuserApp::typeToString(Type type) {
  switch (type) {
  case ByEar:
    return "ByEar";
    break;
  case Linear:
    return "Linear";
    break;
  }
  return "Error";
}