#include "DeviceManager.h"
#include <basedevice.h>
#include <indiproperty.h>
#include <tuple>
#include <algorithm>

void DeviceManager::addCamera(const Camera &camera) { cameras.push_back(camera); }
void DeviceManager::addFocuser(const Focuser &focuser) { focusers.push_back(focuser); };

std::vector<Camera> &DeviceManager::getCameras() { return cameras; };
std::vector<Focuser> &DeviceManager::getFocusers() { return focusers; };

bool DeviceManager::isReady() const {
  auto areCamerasInitialized = std::ranges::all_of(cameras, isCameraInitialized);
  auto hasFocusers = focusers.size() > 0;
  return areCamerasInitialized && hasFocusers;
}

bool DeviceManager::isCameraInitialized(const Camera &camera) {
  return camera.width > 0 && camera.height > 0;
}

////////////////////////////////////////
// camera //////////////////////////////
////////////////////////////////////////

std::tuple<int, int> DeviceManager::updateCameraResolution(const INDI::BaseDevice &device) {
  auto &camera = findCameraByName(device.getDeviceName());
  std::tie(camera.width, camera.height) = getCameraResolution(device);

  std::ranges::sort(cameras, compareCameraResolutions);

  return {camera.width, camera.height};
}

Camera &DeviceManager::findCameraByName(const std::string_view &name) {
  return *std::ranges::find_if(cameras, [&name](const Camera &x) {
    return x.name == name;
  });
}

std::tuple<int, int> DeviceManager::getCameraResolution(const INDI::BaseDevice &device) {
  const auto ccdInfo = device.getNumber("CCD_INFO");

  auto maxX = ccdInfo.findWidgetByName("CCD_MAX_X");
  auto maxY = ccdInfo.findWidgetByName("CCD_MAX_Y");

  int width = maxX->getValue();
  int height = maxY->getValue();

  return { width, height };
}

int DeviceManager::compareCameraResolutions(const Camera &a, const Camera &b) {
  return a.width * a.height > b.width * b.height;
}

////////////////////////////////////////
// Focuser /////////////////////////////
////////////////////////////////////////

int DeviceManager::updateFocuserPosition(const INDI::BaseDevice &device) {
  auto &focuser = findFocuserByName(device.getDeviceName());
  auto steps = getFocuserMotion(device);

  focuser.position += steps;

  return focuser.position;
}

Focuser &DeviceManager::findFocuserByName(const std::string_view &name) {
  return *std::ranges::find_if(focusers, [&name](const Focuser &x) {
    return x.name == name;
  });
}


int DeviceManager::getFocuserMotion(const INDI::BaseDevice &device) {
  const auto relFocusPosition = device.getNumber("REL_FOCUS_POSITION");
  const auto steps = relFocusPosition.at(0)->getValue();

  auto focusMotion = device.getSwitch("FOCUS_MOTION");
  auto focusInward = focusMotion.findWidgetByName("FOCUS_INWARD");
  auto focusOutward = focusMotion.findWidgetByName("FOCUS_OUTWARD");

  if (focusInward->getState() == ISS_ON) {
    return -steps;
  } else if (focusOutward->getState() == ISS_ON) {
    return steps;
  }

  return 0;
}
