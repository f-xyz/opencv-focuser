#include "./INDIClient.h"
#include "../FitsReader/FitsReader.h"
#include <cstring>
#include <future>
#include <indiapi.h>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <print>
#include <string>

INDIClient::INDIClient(std::function<void()> callback)
    : onReady(std::move(callback)), throttle(1000, onReady) {
  std::println("Connecting to {}:{}", host, port);

  setServer(host.c_str(), port);
  if (connectServer()) {
    std::println("Initializing");
  } else {
    std::println(stderr, "INDI server connection failed.");
  }
}

INDIClient::~INDIClient() {
  BaseClient::disconnectServer(0);
  std::println("Disconnected");
}

std::future<cv::Mat> INDIClient::shoot(const Camera &camera, double seconds) {
  std::println("\nShooting...");

  imagePromise = std::make_unique<std::promise<cv::Mat>>();

  auto ccd = getDevice(camera.name.c_str());
  auto exposure = ccd.getNumber("CCD_EXPOSURE");
  exposure[0].setValue(seconds);
  sendNewNumber(exposure);

  return imagePromise->get_future();
}

std::future<bool> INDIClient::move(bool isOutwards, int steps) {
  std::println("\nMoving focuser...");

  if (!focusPromise) {
    focusPromise = std::make_unique<std::promise<bool>>();
  }

  auto focuser = focusers.front();
  auto device = getDevice(focuser.name.c_str());
  if (!device.isValid()) {
    std::println("  - Error: device handle is invalid!");
    focusPromise->set_value(false);
    return focusPromise->get_future();
  }

  // Direction
  auto direction = device.getSwitch("FOCUS_MOTION");
  if (!direction.isValid()) {
    std::println("  - Error: direction is invalid!");
    focusPromise->set_value(false);
    return focusPromise->get_future();
  }

  auto directionSwitch = direction.getSwitch();
  directionSwitch->reset();

  auto targetSwitch = isOutwards
    ? directionSwitch->findWidgetByName("FOCUS_OUTWARD")
    : directionSwitch->findWidgetByName("FOCUS_INWARD");

  if (targetSwitch) {
    targetSwitch->setState(ISS_ON);
  } else {
    directionSwitch->at(isOutwards ? 1 : 0)->setState(ISS_ON);
  }

  sendNewSwitch(directionSwitch);
  std::println("  * Direction sent");

  // Position
  auto position = device.getNumber("REL_FOCUS_POSITION");
  if (!position.isValid()) {
    std::println("  - Error: position is invalid!");
    focusPromise->set_value(false);
    return focusPromise->get_future();
  }

  auto positionNumber = position.getNumber();
  positionNumber->at(0)->setValue(steps);
  sendNewNumber(positionNumber);
  std::println("  * Position sent");

  return focusPromise->get_future();
}

void INDIClient::newDevice(INDI::BaseDevice device) {
  auto name = std::string(device.getDeviceName());
  if (name.contains("CCD")) {
    std::println("* New camera: {}", name);
    cameras.push_back(Camera {name});
  } else if (name.contains("Focuser")) {
    std::println("* New focuser: {}", name);
    focusers.push_back(Focuser {name});
  } else {
    std::println("* New device: {}", name);
  }
}

void INDIClient::newProperty(INDI::Property property) {
  auto deviceName = std::string(property.getDeviceName());
  auto propertyName = std::string(property.getName());
  bool isConnection = property.isNameMatch("CONNECTION");
  bool isCCDInfo = property.isNameMatch("CCD_INFO");

  if (property.isNameMatch("CONNECTION")) {
    std::println("  * New property: {} / {}", deviceName, propertyName);
    if (deviceName.contains("CCD")) {
      setBLOBMode(B_ALSO, deviceName.c_str(), nullptr);
      enableDirectBlobAccess(deviceName.c_str(), nullptr);
      connectDevice(deviceName.c_str());
      std::println("    + Camera connected!");
    } else if (deviceName.contains("Focuser")) {
      connectDevice(deviceName.c_str());
      std::println("    + Focuser connected!");
    }
  } else if (isCCDInfo) {
    std::println("  * New property: {} / {}", deviceName, propertyName);
    auto [width, height] = getCameraResolution(property);
    auto &camera = getCameraByName(deviceName);
    camera.width = width;
    camera.height = height;
    std::println("    + Camera resolution: {}x{}", camera.width, camera.height);

    if (isReady()) {
      std::ranges::sort(cameras, compareCameras);
      throttle.call();
    }
  }

  std::println("  * New property {} / {}", deviceName, propertyName);
  // if (property.isNameMatch("FOCUS_RELATIVE_POSITION")) {
  //   std::println("  -> FOCUS_RELATIVE_POSITION is ready!");
  // } else if (property.isNameMatch("FOCUS_ABSOLUTE_POSITION")) {
  //   std::println("  -> Device uses ABSOLUTE position instead of RELATIVE!");
  // }
}

void INDIClient::updateProperty(INDI::Property property) {
  auto deviceName = std::string(property.getDeviceName());
  auto propertyName = std::string(property.getName());
  auto isCCDImage = property.isNameMatch("CCD1");
  auto isRelFocusPosition = property.isNameMatch("REL_FOCUS_POSITION");

  std::println("  * Updated property {} / {}", deviceName, propertyName);

  if (isCCDImage) {
    std::println("* Updated property: {} / CCD_IMAGE", deviceName);
    auto blob = property.getBLOB();

    if (blob->getState() == IPS_OK) {
      std::println("  * Image received!");

      auto item = blob->at(0);
      auto data = item->getBlob();
      auto size = item->getBlobLen();
      auto format = std::string(item->getFormat());
      if (format == ".fits") {
        cv::Mat image = FitsReader().read(data, size);
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
  } else if (isRelFocusPosition) {
    std::println("  * Focuser movement:");
    
    // focusPromise->set_value(true);
    // focusPromise.reset();
    // return;

    auto position = property.getNumber();
    auto state = position->getState();

    switch (state) {
      case IPS_IDLE:
        break;

      case IPS_BUSY:
        std::println("    * Focuser is moving... Current step delta: {}",
          position->at(0)->getValue());
        break;

      case IPS_OK: // Reached position successfully
        std::println("    + Focuser motion complete!");
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
}

void INDIClient::newMessage(INDI::BaseDevice device, int messageId) {
  // auto deviceName = device.getDeviceName();
  // auto message = device.messageQueue(messageId);
  // std::println("* New message: {} / {}", deviceName, message);
}

std::tuple<int, int> INDIClient::getCameraResolution(INDI::Property &property) const {
  auto number = property.getNumber();

  int width = 0;
  int height = 0;

  for (int i = 0; i < number->count(); ++i) {
    if (strcmp(number->np[i].name, "CCD_MAX_X") == 0) {
      width = number->np[i].value;
    } else if (strcmp(number->np[i].name, "CCD_MAX_Y") == 0) {
      height = number->np[i].value;
    }
  }

  return std::tuple(width, height);
}

Camera &INDIClient::getCameraByName(const std::string &name) {
  return *std::ranges::find_if(cameras, [&name](const Camera &x) {
    return x.name == name;
  });
}

bool INDIClient::isReady() const {
  // Are all the cameras initialized?
  return std::ranges::all_of( cameras, [](const Camera &x) {
    return x.width > 0 && x.height > 0;
  });
}

int INDIClient::compareCameras(const Camera &a, const Camera &b) {
  return a.width * a.height > b.width * b.height;
}
