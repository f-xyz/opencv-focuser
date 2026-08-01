#pragma once

#include <CLI11.hpp>
#include <string>

class Config {
public:
  std::string logFilePath = "opencv-focuser.log";

  std::string indiHost = "localhost";
  unsigned int indiPort = 7624u;

  unsigned int nIterations = 5;
  double precision = 0.1;

  double cameraExposure = 1;
  unsigned int cameraAverageFrames = 1;

  unsigned int focuserStepSize = 100;
  unsigned int focuserBacklash = 100;
  unsigned int focuserLimit = 5000;

  bool parse(const int argc, const char **argv);
};