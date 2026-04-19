#include "pch.h"
#include "MiniMap.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CMiniMap::CMiniMap()
{
}

bool CMiniMap::LoadMapImage(const CString& path)
{
    CStringA pathA(path);
    m_mapBgr = cv::imread(std::string(pathA.GetString()), cv::IMREAD_COLOR);
    if (m_mapBgr.empty())
        return false;

    m_centerX = (m_mapBgr.cols - 1) * 0.5;
    m_centerY = (m_mapBgr.rows - 1) * 0.5;
    m_zoom = 1.0;
    ClampCenter();
    return true;
}

void CMiniMap::SetGeoBounds(double latTop, double latBottom, double lonLeft, double lonRight)
{
    m_bounds.latTop = latTop;
    m_bounds.latBottom = latBottom;
    m_bounds.lonLeft = lonLeft;
    m_bounds.lonRight = lonRight;
    m_bounds.valid = true;
}

void CMiniMap::SetCameraGeo(double lat, double lon)
{
    m_cameraLat = lat;
    m_cameraLon = lon;

    double imgX = 0.0, imgY = 0.0;
    if (LatLonToImage(lat, lon, imgX, imgY))
    {
        m_centerX = imgX;
        m_centerY = imgY;
        ClampCenter();
    }
}

void CMiniMap::SetCameraPanTilt(double panDeg, double tiltDeg)
{
    m_panDeg = panDeg;
    m_tiltDeg = tiltDeg;
}

void CMiniMap::SetHFOV(double hfovDeg)
{
    m_hfovDeg = std::max(0.1, hfovDeg);
}

void CMiniMap::SetRangeMode(bool enable)
{
    m_rangeMode = enable;
    if (!enable)
        EndRangeMeasure();
}

void CMiniMap::SetClickMoveEnabled(bool enable)
{
    m_clickMoveEnabled = enable;
}

void CMiniMap::ClearMarkers()
{
    m_markers.clear();
}

void CMiniMap::AddMarker(double lat, double lon, const CString& text, const cv::Scalar& color, int radius)
{
    Marker m;
    m.lat = lat;
    m.lon = lon;
    m.text = text;
    m.color = color;
    m.radius = radius;
    m_markers.push_back(m);
}

void CMiniMap::ResetView()
{
    if (m_mapBgr.empty())
        return;

    m_centerX = (m_mapBgr.cols - 1) * 0.5;
    m_centerY = (m_mapBgr.rows - 1) * 0.5;
    m_zoom = 1.0;
    ClampCenter();
}

void CMiniMap::ClampCenter()
{
    if (m_mapBgr.empty())
        return;

    m_centerX = clamp(m_centerX, 0.0, (double)std::max(0, m_mapBgr.cols - 1));
    m_centerY = clamp(m_centerY, 0.0, (double)std::max(0, m_mapBgr.rows - 1));
}

void CMiniMap::ZoomIn()
{
    m_zoom = std::min(m_zoom + 0.2, m_maxZoom);
    ClampCenter();
}

void CMiniMap::ZoomOut(int viewW, int viewH)
{
    const double fitMinZoom = GetFitMinZoom(viewW, viewH);
    const double minZoom = std::max(m_minZoom, fitMinZoom);

    m_zoom = std::max(m_zoom - 0.2, minZoom);
    ClampCenter(viewW, viewH);
}

void CMiniMap::ZoomAt(int viewX, int viewY, int viewW, int viewH, double delta)
{
    if (m_mapBgr.empty() || viewW <= 0 || viewH <= 0)
        return;

    const double beforeX = m_centerX + (viewX - viewW * 0.5) / m_zoom;
    const double beforeY = m_centerY + (viewY - viewH * 0.5) / m_zoom;

    const double fitMinZoom = GetFitMinZoom(viewW, viewH);
    const double minZoom = std::max(m_minZoom, fitMinZoom);

    m_zoom = clamp(m_zoom + delta, minZoom, m_maxZoom);

    m_centerX = beforeX - (viewX - viewW * 0.5) / m_zoom;
    m_centerY = beforeY - (viewY - viewH * 0.5) / m_zoom;

    ClampCenter(viewW, viewH);
}

void CMiniMap::MoveCenterByPixel(int dxImg, int dyImg)
{
    if (m_mapBgr.empty())
        return;

    m_centerX += dxImg;
    m_centerY += dyImg;
    ClampCenter();
}

bool CMiniMap::LatLonToImage(double lat, double lon, double& imgX, double& imgY) const
{
    if (m_mapBgr.empty() || !m_bounds.valid)
        return false;

    const double lonSpan = m_bounds.lonRight - m_bounds.lonLeft;
    const double latSpan = m_bounds.latTop - m_bounds.latBottom;
    if (std::abs(lonSpan) < 1e-12 || std::abs(latSpan) < 1e-12)
        return false;

    imgX = (lon - m_bounds.lonLeft) / lonSpan * (m_mapBgr.cols - 1);
    imgY = (m_bounds.latTop - lat) / latSpan * (m_mapBgr.rows - 1);
    return true;
}

bool CMiniMap::ImageToLatLon(double imgX, double imgY, double& lat, double& lon) const
{
    if (m_mapBgr.empty() || !m_bounds.valid)
        return false;

    const double xNorm = imgX / std::max(1, m_mapBgr.cols - 1);
    const double yNorm = imgY / std::max(1, m_mapBgr.rows - 1);

    lon = m_bounds.lonLeft + xNorm * (m_bounds.lonRight - m_bounds.lonLeft);
    lat = m_bounds.latTop - yNorm * (m_bounds.latTop - m_bounds.latBottom);
    return true;
}

bool CMiniMap::ViewPointToLatLon(int x, int y, int viewW, int viewH, double& lat, double& lon) const
{
    if (m_mapBgr.empty() || viewW <= 0 || viewH <= 0)
        return false;

    const double imgX = m_centerX + (x - viewW * 0.5) / m_zoom;
    const double imgY = m_centerY + (y - viewH * 0.5) / m_zoom;
    return ImageToLatLon(imgX, imgY, lat, lon);
}

bool CMiniMap::LatLonToViewPoint(double lat, double lon, int viewW, int viewH, CPoint& outPt) const
{
    double imgX = 0.0, imgY = 0.0;
    if (!LatLonToImage(lat, lon, imgX, imgY))
        return false;

    outPt.x = (int)std::round((imgX - m_centerX) * m_zoom + viewW * 0.5);
    outPt.y = (int)std::round((imgY - m_centerY) * m_zoom + viewH * 0.5);
    return true;
}

bool CMiniMap::BeginRangeMeasure(int x, int y, int viewW, int viewH)
{
    if (!ViewPointToLatLon(x, y, viewW, viewH, m_measureLat1, m_measureLon1))
        return false;

    m_measureLat2 = m_measureLat1;
    m_measureLon2 = m_measureLon1;
    m_measuredMeters = 0.0;
    m_hasMeasure = true;
    return true;
}

bool CMiniMap::UpdateRangeMeasure(int x, int y, int viewW, int viewH)
{
    if (!m_hasMeasure)
        return false;

    if (!ViewPointToLatLon(x, y, viewW, viewH, m_measureLat2, m_measureLon2))
        return false;

    m_measuredMeters = HaversineMeters(
        m_measureLat1, m_measureLon1,
        m_measureLat2, m_measureLon2);

    return true;
}

void CMiniMap::EndRangeMeasure()
{
    // 측정 결과는 남김
}

void CMiniMap::SetHoverPoint(int x, int y, int viewW, int viewH)
{
    double lat = 0.0, lon = 0.0;
    if (ViewPointToLatLon(x, y, viewW, viewH, lat, lon))
    {
        m_hoverText.Format(L"LAT %.6f  LON %.6f", lat, lon);
    }
    else
    {
        m_hoverText.Empty();
    }
}

bool CMiniMap::IsCameraOutOfRange() const
{
    if (!m_bounds.valid)
        return false;

    if (m_cameraLat > std::max(m_bounds.latTop, m_bounds.latBottom)) return true;
    if (m_cameraLat < std::min(m_bounds.latTop, m_bounds.latBottom)) return true;
    if (m_cameraLon < std::min(m_bounds.lonLeft, m_bounds.lonRight)) return true;
    if (m_cameraLon > std::max(m_bounds.lonLeft, m_bounds.lonRight)) return true;
    return false;
}

void CMiniMap::ClampCenter(int viewW, int viewH)
{
    if (m_mapBgr.empty())
        return;

    if (viewW <= 0 || viewH <= 0)
    {
        m_centerX = clamp(m_centerX, 0.0, (double)std::max(0, m_mapBgr.cols - 1));
        m_centerY = clamp(m_centerY, 0.0, (double)std::max(0, m_mapBgr.rows - 1));
        return;
    }

    const double halfW = (viewW * 0.5) / m_zoom;
    const double halfH = (viewH * 0.5) / m_zoom;

    const double minX = halfW;
    const double maxX = (double)m_mapBgr.cols - halfW;
    const double minY = halfH;
    const double maxY = (double)m_mapBgr.rows - halfH;

    // zoom out 해서 화면이 맵보다 커질 수 있는 경우도 방어
    if (minX > maxX)
        m_centerX = (m_mapBgr.cols - 1) * 0.5;
    else
        m_centerX = clamp(m_centerX, minX, maxX);

    if (minY > maxY)
        m_centerY = (m_mapBgr.rows - 1) * 0.5;
    else
        m_centerY = clamp(m_centerY, minY, maxY);
}

bool CMiniMap::Render(int viewW, int viewH, cv::Mat& outBgr)
{
    if (m_mapBgr.empty() || viewW <= 0 || viewH <= 0)
        return false;

    const double fitMinZoom = GetFitMinZoom(viewW, viewH);
    if (m_zoom < fitMinZoom)
        m_zoom = fitMinZoom;

    ClampCenter(viewW, viewH);

    outBgr.create(viewH, viewW, CV_8UC3);

    for (int y = 0; y < viewH; ++y)
    {
        cv::Vec3b* dstRow = outBgr.ptr<cv::Vec3b>(y);
        for (int x = 0; x < viewW; ++x)
        {
            const double srcX = m_centerX + (x - viewW * 0.5) / m_zoom;
            const double srcY = m_centerY + (y - viewH * 0.5) / m_zoom;

            int ix = (int)std::round(srcX);
            int iy = (int)std::round(srcY);

            ix = std::max(0, std::min(ix, m_mapBgr.cols - 1));
            iy = std::max(0, std::min(iy, m_mapBgr.rows - 1));

            dstRow[x] = m_mapBgr.at<cv::Vec3b>(iy, ix);
        }
    }

    return true;
}

void CMiniMap::DrawCamera(cv::Mat& dst, int viewW, int viewH) const
{
    CPoint pt;
    if (!LatLonToViewPoint(m_cameraLat, m_cameraLon, viewW, viewH, pt))
        return;

    if (IsCameraOutOfRange())
    {
        cv::putText(dst, "OUT OF RANGE", cv::Point(20, 40),
            cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 0, 255), 2);
        return;
    }

    cv::circle(dst, cv::Point(pt.x, pt.y), 6, cv::Scalar(0, 255, 255), -1);
    cv::circle(dst, cv::Point(pt.x, pt.y), 10, cv::Scalar(0, 0, 0), 1);

    const double leftDeg = NormalizeDeg(m_panDeg - m_hfovDeg * 0.5);
    const double rightDeg = NormalizeDeg(m_panDeg + m_hfovDeg * 0.5);

    const int radius = 80;
    cv::Point p0(pt.x, pt.y);
    cv::Point p1(
        pt.x + (int)std::round(std::sin(leftDeg * M_PI / 180.0) * radius),
        pt.y - (int)std::round(std::cos(leftDeg * M_PI / 180.0) * radius));
    cv::Point p2(
        pt.x + (int)std::round(std::sin(rightDeg * M_PI / 180.0) * radius),
        pt.y - (int)std::round(std::cos(rightDeg * M_PI / 180.0) * radius));

    std::vector<cv::Point> poly;
    poly.push_back(p0);
    poly.push_back(p1);

    const int segs = 18;
    for (int i = 1; i < segs; ++i)
    {
        const double t = (double)i / (double)segs;
        double deg = leftDeg + (rightDeg - leftDeg) * t;
        if (std::abs(rightDeg - leftDeg) > 180.0)
        {
            double a = leftDeg;
            double b = rightDeg;
            if (b < a) b += 360.0;
            deg = a + (b - a) * t;
            if (deg > 180.0) deg -= 360.0;
        }

        poly.push_back(cv::Point(
            pt.x + (int)std::round(std::sin(deg * M_PI / 180.0) * radius),
            pt.y - (int)std::round(std::cos(deg * M_PI / 180.0) * radius)));
    }

    poly.push_back(p2);

    cv::Mat overlay = dst.clone();
    cv::fillConvexPoly(overlay, poly, cv::Scalar(0, 180, 255));
    cv::addWeighted(overlay, 0.18, dst, 0.82, 0.0, dst);

    cv::line(dst, p0, p1, cv::Scalar(0, 180, 255), 1);
    cv::line(dst, p0, p2, cv::Scalar(0, 180, 255), 1);

    cv::Point heading(
        pt.x + (int)std::round(std::sin(m_panDeg * M_PI / 180.0) * (radius + 10)),
        pt.y - (int)std::round(std::cos(m_panDeg * M_PI / 180.0) * (radius + 10)));

    cv::arrowedLine(dst, p0, heading, cv::Scalar(0, 255, 0), 2, cv::LINE_AA, 0, 0.2);
}

void CMiniMap::DrawMarkers(cv::Mat& dst, int viewW, int viewH) const
{
    for (const auto& m : m_markers)
    {
        CPoint pt;
        if (!LatLonToViewPoint(m.lat, m.lon, viewW, viewH, pt))
            continue;

        cv::circle(dst, cv::Point(pt.x, pt.y), m.radius, m.color, -1);

        CStringA txtA(m.text);
        cv::putText(dst, std::string(txtA.GetString()),
            cv::Point(pt.x + 8, pt.y - 8),
            cv::FONT_HERSHEY_SIMPLEX, 0.45, m.color, 1);
    }
}

void CMiniMap::DrawMeasure(cv::Mat& dst, int viewW, int viewH) const
{
    if (!m_hasMeasure)
        return;

    CPoint p1, p2;
    if (!LatLonToViewPoint(m_measureLat1, m_measureLon1, viewW, viewH, p1))
        return;
    if (!LatLonToViewPoint(m_measureLat2, m_measureLon2, viewW, viewH, p2))
        return;

    cv::line(dst, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y), cv::Scalar(255, 255, 0), 2);
    cv::circle(dst, cv::Point(p1.x, p1.y), 4, cv::Scalar(255, 255, 0), -1);
    cv::circle(dst, cv::Point(p2.x, p2.y), 4, cv::Scalar(255, 255, 0), -1);

    char buf[64] = {};
    sprintf_s(buf, "%.1f m", m_measuredMeters);
    cv::putText(dst, buf, cv::Point((p1.x + p2.x) / 2 + 8, (p1.y + p2.y) / 2 - 8),
        cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 2);
}

void CMiniMap::DrawHud(cv::Mat& dst) const
{
    if (!m_hoverText.IsEmpty())
    {
        CStringA txtA(m_hoverText);
        cv::putText(dst, std::string(txtA.GetString()), cv::Point(12, dst.rows - 16),
            cv::FONT_HERSHEY_SIMPLEX, 0.48, cv::Scalar(255, 255, 255), 1);
    }

    char buf[128] = {};
    sprintf_s(buf, "Zoom %.2f  Pan %.1f  Tilt %.1f  HFOV %.2f",
        m_zoom, m_panDeg, m_tiltDeg, m_hfovDeg);
    cv::putText(dst, buf, cv::Point(12, 24),
        cv::FONT_HERSHEY_SIMPLEX, 0.48, cv::Scalar(255, 255, 255), 1);
}

double CMiniMap::NormalizeDeg(double deg)
{
    while (deg > 180.0) deg -= 360.0;
    while (deg < -180.0) deg += 360.0;
    return deg;
}

double CMiniMap::HaversineMeters(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000.0;

    const double p1 = lat1 * M_PI / 180.0;
    const double p2 = lat2 * M_PI / 180.0;
    const double dp = (lat2 - lat1) * M_PI / 180.0;
    const double dl = (lon2 - lon1) * M_PI / 180.0;

    const double a =
        std::sin(dp * 0.5) * std::sin(dp * 0.5) +
        std::cos(p1) * std::cos(p2) *
        std::sin(dl * 0.5) * std::sin(dl * 0.5);

    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return R * c;
}

double CMiniMap::GetFitMinZoom(int viewW, int viewH) const
{
    if (m_mapBgr.empty() || viewW <= 0 || viewH <= 0)
        return 1.0;

    const double zoomX = (double)viewW / (double)std::max(1, m_mapBgr.cols);
    const double zoomY = (double)viewH / (double)std::max(1, m_mapBgr.rows);

    // SubView를 완전히 덮을 수 있는 최소 zoom
    return std::max(zoomX, zoomY);
}