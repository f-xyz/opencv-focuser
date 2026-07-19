#include <abstractbaseclient.h>
#include <algorithm>
#include <basedevice.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <indiapi.h>
#include <indibase.h>
#include <indibasetypes.h>
#include <indidevapi.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/core/cvstd.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <string>
#include <libindi/baseclient.h>
#include <libindi/indiproperty.h>
#include <libindi/defaultdevice.h>
#include <tuple>
#include <utility>
#include <opencv2/highgui.hpp>
#include "FitsReader/FitsReader.h"
#include "SharpnessEstimator/SharpnessEstimator.h"
#include "utils/utils.h"
#include "utils/colors.h"

namespace fs = std::filesystem;

cv::Mat readFile(const std::string &file) {
  auto ext = fs::path(file).extension().string();
  if (ext == ".fit" || ext == ".fits") {
    FitsReader fits;
    cv::Mat image = fits.read(file);
    return image;
  } else {
    return cv::imread(file);
  }
}

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::println("{}", rgb("OpenCV Focuser", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  struct Camera {
    std::string name = "";
    int width = 0;
    int height = 0;
  };

  class INDIClient : public INDI::BaseClient {
    public:
      std::string host = "localhost";
      unsigned int port = 7624;
      std::vector<Camera> cameras;
      std::function<void()> onReady;
      Throttle trottle;

      INDIClient(std::function<void()> callback) :
        onReady(std::move(callback)),
        trottle(1000, onReady) {
          std::println("Connecting to {}:{}", host, port);

          setServer(host.c_str(), port);
          auto isConnected = connectServer();

          if (isConnected) {
            std::println("Initializing");
          } else {
            std::println(stderr, "INDI server connection failed.");
          }
        }

      virtual ~INDIClient() {
        disconnectServer(0);
        std::println("Disconnected");
      }

      void shoot(const Camera &camera, double seconds) {
        auto ccd = getDevice(camera.name.c_str());
        auto exposure = ccd.getNumber("CCD_EXPOSURE");
        exposure[0].setValue(seconds);
        sendNewNumber(exposure);
      }

    protected:
      void newDevice(INDI::BaseDevice device) override {
        auto name = device.getDeviceName();
        if (std::string(name).contains("CCD")) {
          std::println("* New camera: {}", name);
          cameras.push_back(Camera {name});
          setBLOBMode(B_ALSO, name, nullptr);
          enableDirectBlobAccess(name, nullptr);
          connectDevice(name);
        } else {
          std::println("* New device: {}", name);
        }
      }

      void newProperty(INDI::Property property) override {
        auto device = property.getDeviceName();
        bool isCCDInfoProperty = property.isNameMatch("CCD_INFO");
        bool isCCDImageProperty = property.isNameMatch("CCD1");
        bool isNumberProp = property.getType() == INDI_NUMBER;

        if (isCCDInfoProperty && isNumberProp) {
          auto [width, height] = getResolution(property);
          auto &camera = getCameraByName(device);
          camera.width = width;
          camera.height = height;

          std::println("* New property: {} / CCD_INFO / {}x{}",
            camera.name, camera.width, camera.height);

          if (isReady()) {
            std::ranges::sort(cameras, compareCameras);
            trottle.call();
          }
        }

        if (isCCDImageProperty) {
          std::println("* New property: {} / CCD_IMAGE", device);
          setBLOBMode(B_ALSO, device, "CCD_IMAGE");
          enableDirectBlobAccess(device, "CCD_IMAGE");
        }
      }

      void updateProperty(INDI::Property property) override {
        auto device = property.getDeviceName();
        auto isCCDExposure = property.isNameMatch("CCD_EXPOSURE");
        auto isCCDImage = property.isNameMatch("CCD1");

        if (isCCDExposure) {
          std::println("* Updated property: {} / CCD_EXPOSURE", device);
          auto exposure = property.getNumber();
          auto state = exposure->getState();
          switch (state) {
            case IPS_OK:
            std::println("Exposure is ready!");
              break;
            case IPS_ALERT:
              std::println("Exposure has failed!");
              break;
            case IPS_IDLE:
            case IPS_BUSY:
              break;
          }
        }

        if (isCCDImage) {
          std::println("* Updated property: {} / CCD_IMAGE", device);
          auto blob = property.getBLOB();
          if (blob->getState() == IPS_OK) {
            auto item = blob->at(0);
            auto data = item->getBlob();
            auto size = item->getBlobLen();
            auto format = item->getFormat(); // .fits

            cv::Mat image = FitsReader().read(data, size);
            cv::imshow("Exposure", image);
            cv::waitKey(0);
            cv::destroyAllWindows();
            exit(0);
          }
        }
      }

      std::tuple<int, int> getResolution(INDI::Property &property) {
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

      Camera &getCameraByName(const char *name) {
        return *std::ranges::find_if(cameras, [&name](const Camera &x) {
          return x.name == name;
        });
      }

      bool isReady() {
        // All cameras are initialized
        return std::ranges::all_of(cameras, [](const Camera &x) {
          return x.width > 0 && x.height > 0;
        });
      }

      static int compareCameras(const Camera &a, const Camera &b) {
        return a.width * a.height > b.width * b.height;
      }
  };

  //////////////////////////////////////

  INDIClient indi([&indi]() {
    std::println("\nCameras:");
    for (auto &x : indi.cameras) {
      std::println("  * {}: {}x{}", x.name, x.width, x.height);
    }

    std::println("\nShooting...");
    indi.shoot(indi.cameras.front(), 0.01);
  });

  std::cin.get();
  return 0;

  //////////////////////////////////////

  const std::string dir = joinArgs(nArgs, args);
  const std::vector files = readDir(dir);

  for (auto &file : files) {
    const cv::Mat image = readFile(file);

    const int w = image.cols;
    const int h = image.rows;
    const auto rect = cv::Rect(w / 4, h / 4, w / 2, h / 2);
    const auto roi = image(rect);

    // cv::Mat preview;
    // cv::resize(image, preview, cv::Size(640, 480));
    // cv::imshow("Image", image);
    // cv::waitKey(0);

    SharpnessEstimator estimator;
    auto sharpness = estimator.getSharpnessGaussian(roi);
    auto name = fs::path(file).filename().string();
    std::println("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}