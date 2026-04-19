#include "pch.h"
#include "CvPainter.h"

void CCvPainter::DrawMatToControl(CStatic* pCtrl, const cv::Mat& mat)
{
    if (!pCtrl || !::IsWindow(pCtrl->GetSafeHwnd()) || mat.empty())
        return;

    CRect rc;
    pCtrl->GetClientRect(&rc);

    const int dstW = rc.Width();
    const int dstH = rc.Height();
    if (dstW <= 0 || dstH <= 0)
        return;

    cv::Mat bgr;
    if (mat.channels() == 1)
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
    else if (mat.channels() == 3)
        bgr = mat;
    else if (mat.channels() == 4)
        cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    else
        return;

    cv::Mat bgra;
    cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bgra.cols;
    bmi.bmiHeader.biHeight = -bgra.rows;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    CDC* pDC = pCtrl->GetDC();
    if (!pDC)
        return;

    SetStretchBltMode(pDC->GetSafeHdc(), COLORONCOLOR);

    StretchDIBits(
        pDC->GetSafeHdc(),
        0, 0, dstW, dstH,
        0, 0, bgra.cols, bgra.rows,
        bgra.data,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);

    pCtrl->ReleaseDC(pDC);
}