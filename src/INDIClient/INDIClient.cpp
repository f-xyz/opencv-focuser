#include "./INDIClient.h"
#include "../FitsReader/FitsReader.h"
#include <future>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

INDIClient::INDIClient(std::function<void()> callback)
    : onReady(std::move(callback)), trottle(1000, onReady) {
  std::println("Connecting to {}:{}", host, port);

  setServer(host.c_str(), port);
  if (BaseClient::connectServer()) {
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
  imagePromise = std::make_unique<std::promise<cv::Mat>>();

  auto ccd = getDevice(camera.name.c_str());
  auto exposure = ccd.getNumber("CCD_EXPOSURE");
  exposure[0].setValue(seconds);
  sendNewNumber(exposure);

  return imagePromise->get_future();
}

void INDIClient::newDevice(INDI::BaseDevice device) {
  auto name = std::string(device.getDeviceName());
  if (name.contains("CCD")) {
    std::println("* New camera: {}", name);
    cameras.push_back(Camera{name});
  } else if (name.contains("Focuser")) {
    std::println("* New focuser: {}", name);
  } else {
    std::println("* New device: {}", name);
  }
}

void INDIClient::newProperty(INDI::Property property) {
  auto device = property.getDeviceName();
  bool isConnectionProperty = property.isNameMatch("CONNECTION");
  bool isCCDInfoProperty = property.isNameMatch("CCD_INFO");

  if (isConnectionProperty) {
    std::println("  * New property: {} / {}", device, property.getName());
    setBLOBMode(B_ALSO, device, nullptr);
    enableDirectBlobAccess(device, nullptr);
    connectDevice(device);
  } else if (isCCDInfoProperty) {
    auto [width, height] = getResolution(property);
    auto &camera = getCameraByName(device);
    camera.width = width;
    camera.height = height;

    std::println("  * New property: {} / CCD_INFO / {}x{}",
      camera.name, camera.width, camera.height);

    if (isReady()) {
      std::ranges::sort(cameras, compareCameras);
      trottle.call();
    }
  }
}

void INDIClient::updateProperty(INDI::Property property) {
  auto device = property.getDeviceName();
  auto isCCDImage = property.isNameMatch("CCD1");

  if (isCCDImage) {
    std::println("* Updated property: {} / CCD_IMAGE", device);
    auto blob = property.getBLOB();

    if (blob->getState() == IPS_OK) {
      std::println("* Image received!");

      auto item = blob->at(0);
      auto data = item->getBlob();
      auto size = item->getBlobLen();
      // auto format = item->getFormat(); // -> ".fits"
      cv::Mat image = FitsReader().read(data, size);
      imagePromise->set_value(image);
      imagePromise.reset();
    } else {
      std::println("* Image failed!");
      imagePromise->set_value({});
      imagePromise.reset();
    }
  }
}

std::tuple<int, int> INDIClient::getResolution(INDI::Property &property) {
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

Camera &INDIClient::getCameraByName(const char *name) {
  return *std::ranges::find_if(cameras, [&name](const Camera &x) { 
    return x.name == name;
  });
}

bool INDIClient::isReady() {
  // Are all cameras initialized?
  return std::ranges::all_of( cameras, [](const Camera &x) {
    return x.width > 0 && x.height > 0; 
  });
}

int INDIClient::compareCameras(const Camera &a, const Camera &b) {
  return a.width * a.height > b.width * b.height;
}
