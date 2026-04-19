#pragma once
#include <windows.h>

// wParam = channel index (0,1,2...)
// lParam = (cv::Mat*) heap pointer (new cv::Mat)
constexpr UINT WM_VIDEOVIEW_FRAME = WM_APP + 100;