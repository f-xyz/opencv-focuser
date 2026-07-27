#pragma once

#include <CLI11.hpp>
#include <string>

class Config {
public:
  std::string logFilePath = "opencv-focuser.log";

  std::string indiHost = "localhost";
  unsigned int indiPort = 7624u;

  double cameraExposure = 0.02;
  unsigned int cameraAverageFrames = 3;

  unsigned int focuserStepSize = 500;
  unsigned int focuserBacklash = 100;
  unsigned int nIterations = 10;

  bool parse(const int argc, const char **argv);
};