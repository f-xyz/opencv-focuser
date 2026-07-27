#include "Config.h"

bool Config::parse(const int argc, const char **argv) {
  CLI::App app {};

  app.add_option("-l,--log",
    logFilePath,
    "Path to log output file.")
    ->capture_default_str();

  app.add_option("-H,--host",
    indiHost,
    "INDI server host.")
    ->capture_default_str();

  app.add_option("-p,--port",
    indiPort,
    "INDI server port.")
    ->check(CLI::Range(1u, 65535u))
    ->capture_default_str();

  app.add_option("-e,--exposure",
    cameraExposure,
    "Camera exposure duration in seconds.")
    ->check(CLI::PositiveNumber)
    ->capture_default_str();

  app.add_option("-a,--average",
    cameraAverageFrames,
    "Number of camera images to average.")
    ->check(CLI::PositiveNumber)
    ->capture_default_str();

  app.add_option("-s,--step-size",
    focuserStepSize,
    "Focuser movement step size.")
    ->capture_default_str();

  app.add_option("-b,--backlash",
    focuserBacklash,
    "Focuser backlash compensation steps.")
    ->capture_default_str();

  app.add_option("-n,--iterations",
    nIterations,
    "Number of focus measurement iterations.")
    ->capture_default_str();

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    app.exit(e);
    return false;
  }

  return true;
}
