#include <abstractbaseclient.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <indiapi.h>
#include <indibasetypes.h>
#include <iostream>
#include <opencv2/core/cvstd.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <string>
#include <libindi/baseclient.h>
#include <libindi/indiproperty.h>
#include <libindi/defaultdevice.h>
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
      std::vector<Camera> cameras;

      Camera getBiggestCamera() {
        return cameras[0];
      };

    protected:
      void newDevice(INDI::BaseDevice device) override {
        // std::println("* Device: {}", device.getDeviceName());
      }

      void newProperty(INDI::Property property) override {
        auto deviceName = property.getDeviceName();
        bool isConnection = strcmp(property.getName(), "CONNECTION") == 0;
        bool isCCDInfo = strcmp(property.getName(), "CCD_INFO") == 0;
        bool isNumberProp = property.getType() == INDI_NUMBER;

        if (isConnection) {
          auto connection = property.getSwitch();

          if (connection->sp[0].s == ISS_OFF) {
            connection->sp[0].s = ISS_ON; // Connect to ON
            connection->sp[1].s = ISS_OFF; // Disconnect to OFF
            sendNewSwitch(connection);
          }
        }

        if (isCCDInfo && isNumberProp) {
          auto numberView = property.getNumber();

          int width = 0;
          int height = 0;

          for (int i = 0; i < numberView->count(); ++i) {
            if (strcmp(numberView->np[i].name, "CCD_MAX_X") == 0) {
              width = numberView->np[i].value;
            } else if (strcmp(numberView->np[i].name, "CCD_MAX_Y") == 0) {
              height = numberView->np[i].value;
            }
          }

          cameras.push_back(Camera {
            deviceName,
            width,
            height
          });

          std::ranges::sort(cameras, compare);
          std::println("* {}: {}x{}", deviceName, width, height);
        }
      }

      static bool compare(const Camera &a, const Camera &b) {
        return a.width * a.height > b.width * b.height;
      }
  };

  //////////////////////////////////////

  INDIClient indi;
  indi.setServer("localhost", 7624);
  if (!indi.connectServer()) {
    std::println("Connection to INDI server failed.");
  }

  std::cin.get();
  indi.disconnectServer(0);
  exit(0);

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