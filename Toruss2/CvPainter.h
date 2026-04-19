#pragma once
#include <afxwin.h>
#include <opencv2/opencv.hpp>


class CCvPainter
{
public:
    static void DrawMatToControl(CStatic* pCtrl, const cv::Mat& mat);
};