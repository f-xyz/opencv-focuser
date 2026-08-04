#include "FocuserApp.h"
#include "math/ImageStretcher.h"
#include "utils/colors.h"
#include "utils/utils.h"
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <opencv2/core/hal/interface.h>

FocuserApp::~FocuserApp() {
  cv::destroyAllWindows();
}

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
  logger.info("  Direction: {}", startOutward ? "outward": "inward");
  logger.info("  Focuser step size: {}\n", config.focuserStepSize);

  // Gather data points
  if (type == Type::ByEar) {
    gatherDataByEar(startOutward);
  } else {
    gatherDataLinearly();
  }

  // Find solution
  logger.header("\nSolving...");
  auto solution = solver.findBestPosition();
  solver.clear();

  // Decide what to do next
  return checkSolution(solution);
}

bool FocuserApp::checkSolution(const Solution &solution) {
  logger.header("Checking the solution...");

  switch (solution.type) {
    case Inward: {
      logger.info("Ideal position is: {}\n", bold("inward"));
      focusAbs(solution.bestPoint.position);
      logger.info("");
      autoFocus(Type::ByEar, false);
      break;
    }

    case Outward: {
      logger.info("  Ideal position is: {}\n", bold("outward"));
      focusAbs(solution.bestPoint.position);
      logger.info("");
      autoFocus(Type::ByEar, true);
      break;
    }

    case Around: {
      logger.info("  Ideal position is: {}\n", bold("around"));

      if (validateSolution(solution)) {
        logger.header("Done! @{}", formatNumber(focusPosition));
        std::system("canberra-gtk-play -i complete &");
      } else {
        // Not sharp enough! Trying again with smaller steps...
        if (config.focuserStepSize /= 5) {
          autoFocus(Type::ByEar, false);
        } else {
          logger.error("I did my best... @{}", formatNumber(focusPosition));
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

  auto idealPoint = solution.idealPoint;
  auto bestPoint = solution.bestPoint;

  logger.info("  Ideal sharpness: {}", idealPoint.sharpness);
  logger.info("  Ideal position: {}\n", idealPoint.position);

  // Move to the ideal position
  focusAbs(idealPoint.position);
  logger.info("");

  // Capture an image and compare it with the best
  solver.addPoint(bestPoint.position, bestPoint.sharpness);
  auto result = image(config.cameraExposure);
  logger.info("");

  // Is not worse than the best?
  return result.sharpness >= bestPoint.sharpness;
}

////////////////////////////////////////
// Data Gathering //////////////////////
////////////////////////////////////////

void FocuserApp::gatherDataByEar(bool startOutward) {
  bool isFocusingOutward = startOutward;

  for (int i = 0; i < config.nIterations; ++i) {
    logger.info("Iteration #{} of {} @{}",
      i + 1, config.nIterations, formatNumber(focusPosition));

    auto result = image(config.cameraExposure);

    // Skip focusing at the last iteration
    if (i >= config.nIterations - 1) {
      continue;
    }

    // Swap direction if needed
    if (result.delta < 0) { // Just above camera noise
      isFocusingOutward = !isFocusingOutward;
    }

    focusRel(isFocusingOutward, config.focuserStepSize);
    logger.info("");
  }
}

void FocuserApp::gatherDataLinearly() {
  // Move 2 steps inward
  focusRel(false, config.focuserStepSize * 2);
  auto result = image(config.cameraExposure);

  // Move 1 step outward 4 times
  for (int i = 0; i < 4; ++i) {
    logger.info("\nIteration #{} of {} (Linear)", i + 1, config.nIterations);
    focusRel(true, config.focuserStepSize * 2);
    auto result = image(config.cameraExposure);
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

FocuserApp::ImageResult FocuserApp::image(double exposure) {
  cv::Mat image;
  std::vector<double> sharpnesses;

  for (int i = 0; i < config.cameraAverageFrames; ++i) {
    image = indi.image(exposure).get();

    auto roi = getROI(image);
    auto sharpness = estimator.getSharpness(image);
    sharpnesses.push_back(sharpness);

    preview(image);
  }

  auto sum = std::ranges::fold_left( sharpnesses, 0.0, std::plus {});
  auto sharpness = sum / sharpnesses.size();
  auto delta = solver.addPoint(focusPosition, sharpness);

  logger.info("Sharpness: {}; Delta: {}",
    formatNumber(sharpness),
    formatNumber(delta));

  if (sharpness == 0) {
    logger.error("Invalid image: either all white or all black.");
    std::terminate();
  }

  return {
    image,
    sharpness,
    delta
  };
}

int FocuserApp::focusRel(bool isOutward, unsigned int steps) {
  focusCheckLimits(isOutward, steps);

  if (isOutward) {
    focusPosition = indi.focus(true, steps).get();
  } else {
    // Move inward
    int inwardSteps = steps + config.focuserBacklash;
    focusPosition = indi.focus(false, inwardSteps).get();
    // And move outward a bit to compensate the gearbox backlash
    std::this_thread::sleep_for(100ms);
    focusPosition = indi.focus(true, config.focuserBacklash).get();
  }

  // Wait for the vibration to fade
  std::this_thread::sleep_for(1s);

  logger.info("Position: {}", formatNumber(focusPosition));

  return focusPosition;
}

int FocuserApp::focusAbs(int position) {
  int delta = position - focusPosition;
  if (delta != 0) {
    bool isOutward = delta > 0;
    unsigned int steps = std::abs(delta);
    return focusRel(isOutward, steps);
  } else {
    return focusPosition;
  }
}

void FocuserApp::focusCheckLimits(bool isOutward, unsigned int steps) {
  int signedSteps = isOutward ? steps : -steps;
  int nextPosition = focusPosition + signedSteps;

  if (std::abs(nextPosition) > config.focuserLimit) {
    logger.error("Focuser reached its limit!");
    std::terminate();
  }
}

////////////////////////////////////////
// Helpers /////////////////////////////
////////////////////////////////////////

void FocuserApp::preview(const cv::Mat &image) {
  cv::Mat preview;
  cv::Mat roi(getROI(image));

  cv::resize(roi, preview, cv::Size(640, 480));
  cv::flip(preview, preview, 1); // Flip horizontally

  preview.convertTo(preview, CV_8U);

  ImageStretcher stretcher(preview);
  cv::Mat stretched = stretcher.stretch({
    .type = ImageStretcherOptions::CLAHE,
    .claheClipLimit = 2,
    .claheTileSize = 8,
    .asinhFactor = 2,
    .denoiseH = 0
  });

  cv::imshow("Preview", stretched);
  cv::waitKey(1);
}

cv::Mat FocuserApp::getROI(const cv::Mat &image) {
  return image({
    image.cols / 4,
    image.rows / 4,
    image.cols / 2,
    image.rows / 2
  });
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