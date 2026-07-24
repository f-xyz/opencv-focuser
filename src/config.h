#pragma once

#include <string>

class Config {
public:
  // INDI server
  std::string indiHost = "localhost";
  unsigned int indiPort = 7624;
  // General
  std::string logFilePath = "opencv-focuser.log";
  double cameraExposure = 0.02;
  int focuserStepSize = 500;
};
