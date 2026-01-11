#pragma once
#include "../interfaces/ICamera.h"
#include <opencv2/videoio.hpp>

class DesktopCamera : public ICamera {
public:
    DesktopCamera(int deviceID = 0);
    ~DesktopCamera() override;

    bool capture(cv::Mat& frame) override;
    bool isOpened() const override;

private:
    cv::VideoCapture cap;
};
