#pragma once

#include <basedevice.h>
#include <future>
#include <indiapi.h>
#include <opencv2/core/mat.hpp>
#include <string>
#include <baseclient.h>
#include <libindi/indiproperty.h>
#include <libindi/defaultdevice.h>
#include <vector>
#include "../utils/utils.h"

struct Camera {
  std::string name = "";
  int width = 0;
  int height = 0;
};

struct Focuser {
  std::string name = "";
  int position = 0;
};

class INDIClient : public INDI::BaseClient {
  public:
    std::string host = "localhost";
    unsigned int port = 7624;

    std::vector<Camera> cameras;
    std::vector<Focuser> focusers;

    std::function<void()> onReady;
    Throttle throttle;

    std::unique_ptr<std::promise<Camera>> initPromise; // todo
    std::unique_ptr<std::promise<cv::Mat>> imagePromise;
    std::unique_ptr<std::promise<bool>> focusPromise; // todo

  public:
    explicit INDIClient(std::function<void()> callback);
    ~INDIClient() override;

    std::future<cv::Mat> shoot(const Camera &camera, double seconds);
    std::future<bool> move(bool isOutwards, int steps);

  protected:
    void newDevice(INDI::BaseDevice device) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice device, int messageId) override;

    std::tuple<int, int> getCameraResolution(INDI::Property &property) const;
    Camera &getCameraByName(const std::string &name);
    bool isReady() const;

    static int compareCameras(const Camera &a, const Camera &b);
};