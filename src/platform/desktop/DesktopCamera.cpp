#include "DesktopCamera.h"

DesktopCamera::DesktopCamera(int deviceID) {
    cap.open(deviceID);
}

DesktopCamera::~DesktopCamera() {
    if (cap.isOpened()) {
        cap.release();
    }
}

bool DesktopCamera::capture(cv::Mat& frame) {
    if (!cap.isOpened()) return false;
    return cap.read(frame);
}

bool DesktopCamera::isOpened() const {
    return cap.isOpened();
}
