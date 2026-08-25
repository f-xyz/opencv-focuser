#include "./INDIClient.h"
#include "../utils/utils.h"
#include "../fits/FitsReader.h"
#include <exception>
#include <future>
#include <indiapi.h>
#include <opencv2/core/mat.hpp>
#include <string>
#include <string_view>

using namespace std::literals::chrono_literals;

template <typename T>
static std::future<T> getFuture(std::optional<std::promise<T>> &promise) {
  if (promise.has_value()) {
    promise.reset();
  }
  promise.emplace();
  return promise->get_future();
}

////////////////////////////////////////

INDIClient::~INDIClient() {
  BaseClient::disconnectServer(0);
}

std::future<bool> INDIClient::connect(const std::string &host, unsigned int port) {
  logger.info("Connecting to INDI server at {}:{}", host, port);

  auto future = getFuture(connectPromise);

  throttle.emplace(1s, [this]() {
    if (deviceManager.isReady()) {
      connectPromise->set_value(true);
      connectPromise.reset();
    }
  });

  setServer(host.c_str(), port);
  if (!connectServer()) {
    logger.error("Connection to INDI server failed.");
    connectPromise->set_value(false);
    connectPromise.reset();
  }

  return future;
}

std::future<cv::Mat> INDIClient::image(const double seconds) {
  logger.info("Shooting for {} sec", seconds);

  auto future = getFuture(imagePromise);

  const auto camera = deviceManager.getCameras().front();
  const auto device = getDevice(camera.name.c_str());
  const auto exposure = device.getNumber("CCD_EXPOSURE");

  exposure[0].setValue(seconds);
  sendNewNumber(exposure);

  return future;
}

std::future<int> INDIClient::focus(const bool isOutward, const unsigned int steps) {
  logger.info("Focusing {} steps {}", steps, isOutward ? "OUTWARD" : "INWARD");

  auto future = getFuture(focusPromise);

  const auto focuser = deviceManager.getFocusers().front();
  const auto device = getDevice(focuser.name.c_str());

  const auto direction = device.getSwitch("FOCUS_MOTION");
  const auto directionSwitch = direction.getSwitch();

  if (!direction.isValid()) {
    logger.error("Is the focuser device connected and running?");
    std::terminate();
  }

  directionSwitch->reset();
  directionSwitch->at(isOutward ? 1 : 0)->setState(ISS_ON);
  sendNewSwitch(directionSwitch);

  const auto position = device.getNumber("REL_FOCUS_POSITION");
  const auto positionNumber = position.getNumber();
  positionNumber->at(0)->setValue(steps);
  sendNewNumber(positionNumber);

  return future;
}

void INDIClient::reconnectDevice(const std::string_view &deviceName) {
  auto camera = getDevice(deviceName.data());
  auto connection = camera.getSwitch("CONNECTION");

  if (connection.isValid()) {
    // 1. Disconnect
    connection.getSwitch()->reset();
    connection.getSwitch()->at(1)->setState(ISS_ON); // CONNECT_DISCONNECT
    sendNewSwitch(connection);
    // 2. Re-connect to trigger newProperty
    connection.getSwitch()->reset();
    connection.getSwitch()->at(0)->setState(ISS_ON); // CONNECT_CONNECT
    sendNewSwitch(connection);
  }
}

void INDIClient::newDevice(const INDI::BaseDevice device) {
  const std::string_view deviceName(device.getDeviceName());

  if (deviceName.contains("CCD")) {
    logger.info("New camera: {}", deviceName);
    deviceManager.addCamera({deviceName.data()});
  } else if (deviceName.contains("Focuser")) {
    logger.info("New focuser: {}", deviceName);
    deviceManager.addFocuser({deviceName.data()});
    throttle->call();
  }
}

void INDIClient::newProperty(const INDI::Property property) {
  const std::string_view deviceName(property.getDeviceName());
  const std::string_view propertyName(property.getName());
  logger.debug("New property: {} / {}", deviceName, propertyName);

  if (property.isNameMatch("CONNECTION")) {
    onConnection(property);
  } else if (property.isNameMatch("CCD_INFO")) {
    onCameraInfo(property);
  }
}

void INDIClient::updateProperty(const INDI::Property property) {
  const std::string_view deviceName(property.getDeviceName());
  const std::string_view propertyName(property.getName());
  logger.debug("Updated property: {} / {}", deviceName, propertyName);

  if (property.isNameMatch("CCD1")) {
    const auto camera = deviceManager.getCameras().front();
    if (deviceName == camera.name) {
      onCameraImage(property);
    }
  } else if (property.isNameMatch("REL_FOCUS_POSITION")) {
    onFocuserMotion(property);
  }
}

void INDIClient::newMessage(INDI::BaseDevice device, int messageId) {
  std::string_view deviceName(device.getDeviceName());
  std::string_view message(device.messageQueue(messageId));
  logger.debug("Message: {}: {}", deviceName, message);
}

////////////////////////////////////////
// Event handlers //////////////////////
////////////////////////////////////////

void INDIClient::onConnection(const INDI::Property &property) {
  const std::string_view deviceName(property.getDeviceName());
  logger.info("  CONNECTION: {}", deviceName);

  if (deviceName.contains("CCD")) {
    setBLOBMode(B_ALSO, deviceName.data(), nullptr);
    enableDirectBlobAccess(deviceName.data(), nullptr);
    connectDevice(deviceName.data());
    logger.success("  Camera connected");
  } else if (deviceName.contains("Focuser")) {
    connectDevice(deviceName.data());
    logger.success("  Focuser connected");
  }
}

void INDIClient::onCameraInfo(const INDI::Property &property) {
  const std::string_view deviceName(property.getDeviceName());
  logger.info("  CCD_INFO: {}", deviceName);

  auto device = getDevice(property.getDeviceName());
  auto [width, height] = deviceManager.updateCameraResolution(device);

  if (width * height > 0) {
    logger.success("  Camera resolution: {}x{}", width, height);
    throttle->call();
  } else {
    logger.error("  Re-connecting camera: {}", deviceName);
    reconnectDevice(deviceName);
  }
}

void INDIClient::onCameraImage(const INDI::Property &property) {
  const auto blob = property.getBLOB();
  if (blob->getState() == IPS_OK) {
    const auto item = blob->at(0);
    const auto data = item->getBlob();
    const auto size = item->getBlobLen();
    const auto format = std::string(item->getFormat());

    if (format == ".fits") {
      const auto image = FitsReader().read(data, size);
      imagePromise->set_value(std::move(image));
      imagePromise.reset();
    } else {
      logger.error("  Unknown format: {}", format);
    }
  } else {
    logger.error("  Image failed.");
    imagePromise->set_value({});
    imagePromise.reset();
  }
}

void INDIClient::onFocuserMotion(const INDI::Property &property) {
  const auto device = getDevice(property.getDeviceName());
  const auto steps = deviceManager.updateFocuserPosition(device);

  const auto position = property.getNumber();
  switch (position->getState()) {
    case IPS_BUSY:
      break;

    case IPS_OK: // Reached position successfully
      if (focusPromise) {
        focusPromise->set_value(steps);
        focusPromise.reset();
      }
      break;

    case IPS_ALERT: // Error during motion
      logger.error("  Focuser motion has failed.");
      if (focusPromise) {
        focusPromise->set_value(steps);
        focusPromise.reset();
      }
      break;

    case IPS_IDLE:
      break;
  }
}
