#pragma once

#include <string>

class Config {
public:
  std::string logFilePath = "opencv-focuser.log";

  std::string indiHost = "localhost";
  unsigned int indiPort = 7624;
  
  double cameraExposure = 0.02;
  unsigned int focuserStepSize = 500;
  unsigned int focuserBacklash = 100; // not used ATM
  unsigned int nIterations = 10;
};