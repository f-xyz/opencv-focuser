#pragma once

#include <basedevice.h>
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

  bool isReady() const;

  std::tuple<int, int> updateCameraResolution(const INDI::BaseDevice &device);
  int updateFocuserPosition(const INDI::BaseDevice &device);

private:
  Camera &findCameraByName(const std::string_view &name);
  static std::tuple<int, int> getCameraResolution(const INDI::BaseDevice &device);
  static bool isCameraInitialized(const Camera &camera);
  static int compareCameraResolutions(const Camera &a, const Camera &b);

  Focuser &findFocuserByName(const std::string_view &name);
  static int getFocuserMotion(const INDI::BaseDevice &device);
};