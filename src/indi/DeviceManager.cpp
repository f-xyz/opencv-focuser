#include "DeviceManager.h"
#include <indiproperty.h>
#include <tuple>
#include <algorithm>

void DeviceManager::addCamera(const Camera &camera) { cameras.push_back(camera); }
void DeviceManager::addFocuser(const Focuser &focuser) { focusers.push_back(focuser); };

std::vector<Camera> &DeviceManager::getCameras() { return cameras; };
std::vector<Focuser> &DeviceManager::getFocusers() { return focusers; };

std::tuple<int, int> DeviceManager::updateCameraResolution(const INDI::Property &property) {
  auto &camera = findCameraByName(property.getDeviceName());
  std::tie(camera.width, camera.height) = getCameraResolution(property);

  std::ranges::sort(cameras, compareCameraResolutions);

  return {camera.width, camera.height};
}

bool DeviceManager::isReady() const {
  auto areCamerasInitialized = std::ranges::all_of(cameras, isCameraInitialized);
  auto hasFocusers = focusers.size() > 0;
  return areCamerasInitialized && hasFocusers;
}

std::tuple<int, int> DeviceManager::getCameraResolution(const INDI::Property &property) {
  const auto number = property.getNumber();

  int width = 0;
  int height = 0;

  for (int i = 0; i < number->count(); ++i) {
    if (strcmp(number->np[i].name, "CCD_MAX_X") == 0) {
      width = static_cast<int>(number->np[i].value);
    } else if (strcmp(number->np[i].name, "CCD_MAX_Y") == 0) {
      height = static_cast<int>(number->np[i].value);
    }
  }

  return { width, height };
}

Camera &DeviceManager::findCameraByName(const std::string_view &name) {
  return *std::ranges::find_if(cameras, [&name](const Camera &x) {
    return x.name == name;
  });
}

bool DeviceManager::isCameraInitialized(const Camera &camera) {
  return camera.width > 0 && camera.height > 0;
}

int DeviceManager::compareCameraResolutions(const Camera &a, const Camera &b) {
  return a.width * a.height > b.width * b.height;
}
