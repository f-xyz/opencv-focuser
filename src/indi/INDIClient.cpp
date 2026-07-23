#include "./INDIClient.h"
#include "../fits/FitsReader.h"
#include <cstring>
#include <future>
#include <indiapi.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <print>
#include <string>
#include <string_view>

using namespace std::literals::chrono_literals;

////////////////////////////////////////
// Public //////////////////////////////
////////////////////////////////////////

INDIClient::~INDIClient() {
  disconnectServer(0);
  std::println("\nDisconnected");
}

std::future<bool> INDIClient::connect(const std::string &host, const unsigned int port) {
  connectPromise.emplace();
  auto future = connectPromise->get_future();

  throttle.emplace(1s, [this]() {
    bool isReady = deviceManager.isReady();
    if (isReady) {
      connectPromise->set_value(true);
      connectPromise.reset();
    }
  });

  setServer(host.c_str(), port);
  if (connectServer()) {
    std::println("Initializing");
  } else {
    std::println(stderr, "INDI server connection failed.");
    connectPromise->set_value(false);
    connectPromise.reset();
  }

  return future;
}

std::future<cv::Mat> INDIClient::shoot(double seconds) {
  imagePromise.emplace();
  auto future = imagePromise->get_future();

  const auto camera = deviceManager.getCameras().front();
  const auto ccd = getDevice(camera.name.c_str());
  const auto exposure = ccd.getNumber("CCD_EXPOSURE");
  if (!exposure.isValid()) {
    imagePromise->set_value({});
    imagePromise.reset();
    return future;
  }

  exposure[0].setValue(seconds);
  sendNewNumber(exposure);

  return future;
}

std::future<bool> INDIClient::move(const bool isOutward, const int steps) {
  focusPromise.emplace();
  auto future = focusPromise->get_future();

  const auto focuser = deviceManager.getFocusers().front();
  const auto device = getDevice(focuser.name.c_str());
  if (!device.isValid()) {
    std::println("  - Error: device handle is invalid!");
    focusPromise->set_value(false);
    return future;
  }

  // Direction
  const auto direction = device.getSwitch("FOCUS_MOTION");
  if (!direction.isValid()) {
    std::println("  - Error: direction is invalid!");
    focusPromise->set_value(false);
    return future;
  }

  const auto directionSwitch = direction.getSwitch();
  directionSwitch->reset();

  const auto targetSwitch = isOutward
    ? directionSwitch->findWidgetByName("FOCUS_OUTWARD")
    : directionSwitch->findWidgetByName("FOCUS_INWARD");

  if (targetSwitch) {
    targetSwitch->setState(ISS_ON);
  } else {
    directionSwitch->at(isOutward ? 1 : 0)->setState(ISS_ON);
  }

  sendNewSwitch(directionSwitch);
  std::println("  * Direction sent");

  // Position
  const auto position = device.getNumber("REL_FOCUS_POSITION");
  if (!position.isValid()) {
    std::println("  - Error: position is invalid!");
    focusPromise->set_value(false);
    return future;
  }

  const auto positionNumber = position.getNumber();
  positionNumber->at(0)->setValue(steps);
  sendNewNumber(positionNumber);
  std::println("  * Position sent");

  return future;
}

////////////////////////////////////////
// INDIClient overloads ////////////////
////////////////////////////////////////

void INDIClient::newDevice(const INDI::BaseDevice device) {
  const std::string_view deviceName = device.getDeviceName();

  if (deviceName.contains("CCD")) {
    std::println("* New camera: {}", deviceName);
    deviceManager.addCamera({deviceName.data()});
  } else if (deviceName.contains("Focuser")) {
    std::println("* New focuser: {}", deviceName);
    deviceManager.addFocuser({deviceName.data()});
    std::println("  * Calling the Throttle!");
    throttle->call();
  }
}

void INDIClient::newProperty(const INDI::Property property) {
  if (property.isNameMatch("CONNECTION")) {
    onConnection(property);
  } else if (property.isNameMatch("CCD_INFO")) {
    onCameraInfo(property);
  }
}

void INDIClient::updateProperty(const INDI::Property property) {
  if (property.isNameMatch("CCD1")) { // Image
    onCameraImage(property);
  } else if (property.isNameMatch("REL_FOCUS_POSITION")) {
    onFocuserMotion(property);
  }
}

void INDIClient::newMessage(INDI::BaseDevice device, int messageId) {
  // auto deviceName = device.getDeviceName();
  // auto message = device.messageQueue(messageId);
  // std::println("* New message: {} / {}", deviceName, message);
}

////////////////////////////////////////
// Event handlers //////////////////////
////////////////////////////////////////

void INDIClient::onConnection(const INDI::Property &property) {
  const std::string_view deviceName = property.getDeviceName();
  std::println("  * CONNECTION: {}", deviceName);

  if (deviceName.contains("CCD")) {
    setBLOBMode(B_ALSO, deviceName.data(), nullptr);
    enableDirectBlobAccess(deviceName.data(), nullptr);
    connectDevice(deviceName.data());
    std::println("    + Camera connected!");
  } else if (deviceName.contains("Focuser")) {
    connectDevice(deviceName.data());
    std::println("    + Focuser connected!");
  }
}

void INDIClient::onCameraInfo(const INDI::Property &property) {
  const std::string_view deviceName = property.getDeviceName();
  std::println("  * CCD_INFO: {}", deviceName);

  auto [width, height] = deviceManager.updateCameraResolution(property);
  if (width * height > 0) {
    std::println("    + Camera resolution: {}x{}", width, height);
    std::println("    * Calling the Throttle!");
    throttle->call();
  } else {
    std::println("    * Error, re-connecting camera: {}", deviceName);
    reconnectDevice(deviceName);
  }
}

bool INDIClient::reconnectDevice(const std::string_view &deviceName) {
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

  return true;
}

void INDIClient::onCameraImage(const INDI::Property &property) {
  const std::string_view deviceName = property.getDeviceName();
  const std::string_view propertyName = property.getName();
  std::println("* Updated property: {} / CCD_IMAGE", deviceName);

  const auto blob = property.getBLOB();

  if (blob->getState() == IPS_OK) {
    std::println("  + Image received!");

    const auto item = blob->at(0);
    const auto data = item->getBlob();
    const auto size = item->getBlobLen();
    const auto format = std::string(item->getFormat());

    if (format == ".fits") {
      const auto image = FitsReader().read(data, size);
      imagePromise->set_value(image);
      imagePromise.reset();
    } else {
      std::println("    - Error: unknown format: {}", format);
    }
  } else {
    std::println("* Image failed!");
    imagePromise->set_value({});
    imagePromise.reset();
  }
}

void INDIClient::onFocuserMotion(const INDI::Property &property) {
  std::println("  * Focuser movement:");

  const auto position = property.getNumber();
  switch (position->getState()) {
    case IPS_IDLE:
      break;

    case IPS_BUSY:
      std::println("    * Focuser is moving... Current step delta: {}",
        position->at(0)->getValue());
      break;

    case IPS_OK: // Reached position successfully
      std::println("    + Focuser motion complete: {}",
        position->at(0)->getValue());

      if (focusPromise) {
        focusPromise->set_value(true);
        focusPromise.reset();
      }
      break;

    case IPS_ALERT: // Error during motion
      std::println(stderr, "    * Focuser motion failed!");
      if (focusPromise) {
        focusPromise->set_value(false);
        focusPromise.reset();
      }
      break;
    }
}
