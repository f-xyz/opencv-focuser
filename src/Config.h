#pragma once

#include <CLI11.hpp>
#include <string>

class Config {
public:
  std::string logFilePath = "opencv-focuser.log";

  std::string indiHost = "localhost";
  unsigned int indiPort = 7624u;

  unsigned int nIterations = 5;

  double cameraExposure = 1; // Seconds
  unsigned int cameraAverageFrames = 5;

  unsigned int focuserStepSize = 200;
  unsigned int focuserBacklash = 100;
  unsigned int focuserLimit = 5000;

  bool parse(const int argc, const char **argv);
};