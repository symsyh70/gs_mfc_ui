#pragma once
#include <opencv2/opencv.hpp>
#include <cstdint>

struct VideoFrame
{
    cv::Mat bgr;
    int width = 0;
    int height = 0;
    int64_t pts = 0;
    int64_t frameNumber = 0;

    bool IsValid() const
    {
        return !bgr.empty() && width > 0 && height > 0;
    }

    void Clear()
    {
        bgr.release();
        width = 0;
        height = 0;
        pts = 0;
        frameNumber = 0;
    }
};