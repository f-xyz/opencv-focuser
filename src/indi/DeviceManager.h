#pragma once

#include <indiproperty.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

struct Camera {
  std::string name;
  int width = 0;
  int height = 0;
};

struct Focuser {
  std::string name;
  int position = 0;
};

class DeviceManager final {
  std::vector<Camera> cameras;
  std::vector<Focuser> focusers;

public:
  void addCamera(const Camera &camera);
  void addFocuser(const Focuser &focuser);

  std::vector<Camera> &getCameras();
  std::vector<Focuser> &getFocusers();

  std::tuple<int, int> updateCameraResolution(const INDI::Property &property);
  bool isReady() const;

private:
  Camera &findCameraByName(const std::string_view &name);
  static std::tuple<int, int> getCameraResolution(const INDI::Property &property);
  static bool isCameraInitialized(const Camera &camera);
  static int compareCameraResolutions(const Camera &a, const Camera &b);
};