#pragma once

#include "../logging/Logger.h"
#include "../utils/Throttle.h"
#include "DeviceManager.h"
#include <baseclient.h>
#include <basedevice.h>
#include <future>
#include <libindi/indiproperty.h>
#include <opencv2/core/mat.hpp>
#include <string>
#include <string_view>

class INDIClient final : private INDI::BaseClient {
  Logger &logger;
  DeviceManager deviceManager;
  std::optional<Throttle> throttle;
  std::optional<std::promise<bool>> connectPromise;
  std::optional<std::promise<cv::Mat>> imagePromise;
  std::optional<std::promise<int>> focusPromise;

public:
  INDIClient(Logger &logger) : logger(logger) {}
  virtual ~INDIClient() override;

  std::future<bool> connect(const std::string &host, const unsigned int port);
  std::future<cv::Mat> shoot(double seconds);
  std::future<int> focus(bool isOutward, int steps);

  auto getCameras() { return deviceManager.getCameras(); }
  auto getFocusers() { return deviceManager.getFocusers(); }

private:
  void reconnectDevice(const std::string_view &deviceName);

  void newDevice(INDI::BaseDevice device) override;
  void newProperty(INDI::Property property) override;
  void updateProperty(INDI::Property property) override;
  void newMessage(INDI::BaseDevice device, int messageId) override;

  void onConnection(const INDI::Property &property);
  void onCameraInfo(const INDI::Property &property);
  void onCameraImage(const INDI::Property &property);
  void onFocuserMotion(const INDI::Property &property);
};