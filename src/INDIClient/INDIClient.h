#pragma once

#include <future>
#include <opencv2/core/mat.hpp>
#include <string>
#include <baseclient.h>
#include <libindi/indiproperty.h>
#include <libindi/defaultdevice.h>
#include "../utils/utils.h"

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

    std::unique_ptr<std::promise<cv::Mat>> initPromise;
    std::unique_ptr<std::promise<cv::Mat>> imagePromise;

    explicit INDIClient(std::function<void()> callback);
    ~INDIClient() override;

    std::future<cv::Mat> shoot(const Camera &camera, double seconds);

  protected:
    void newDevice(INDI::BaseDevice device) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;

    std::tuple<int, int> getResolution(INDI::Property &property);
    Camera &getCameraByName(const char *name);
    bool isReady();

    static int compareCameras(const Camera &a, const Camera &b);
};