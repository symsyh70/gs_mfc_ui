#pragma once
#include <opencv2/opencv.hpp>
#include <afxwin.h>
#include <vector>
#include <string>

class CMiniMap
{
public:
    struct GeoBounds
    {
        double latTop = 0.0;
        double latBottom = 0.0;
        double lonLeft = 0.0;
        double lonRight = 0.0;
        bool valid = false;
    };

    struct Marker
    {
        double lat = 0.0;
        double lon = 0.0;
        CString text;
        cv::Scalar color = cv::Scalar(0, 255, 255);
        int radius = 5;
    };

public:
    CMiniMap();

    bool LoadMapImage(const CString& path);
    bool Render(int viewW, int viewH, cv::Mat& outBgr);

    void SetGeoBounds(double latTop, double latBottom, double lonLeft, double lonRight);
    bool HasGeoBounds() const { return m_bounds.valid; }

    void SetCameraGeo(double lat, double lon);
    void SetCameraPanTilt(double panDeg, double tiltDeg);
    void SetHFOV(double hfovDeg);

    void SetRangeMode(bool enable);
    bool IsRangeMode() const { return m_rangeMode; }

    void SetClickMoveEnabled(bool enable);
    bool IsClickMoveEnabled() const { return m_clickMoveEnabled; }

    void ClearMarkers();
    void AddMarker(double lat, double lon, const CString& text, const cv::Scalar& color, int radius = 5);

    void ResetView();
    void ZoomIn();
    void ZoomOut(int viewW, int viewH);
    void ZoomAt(int viewX, int viewY, int viewW, int viewH, double delta);
    void MoveCenterByPixel(int dxImg, int dyImg);

    double GetZoom() const { return m_zoom; }

    bool ViewPointToLatLon(int x, int y, int viewW, int viewH, double& lat, double& lon) const;
    bool LatLonToViewPoint(double lat, double lon, int viewW, int viewH, CPoint& outPt) const;

    bool BeginRangeMeasure(int x, int y, int viewW, int viewH);
    bool UpdateRangeMeasure(int x, int y, int viewW, int viewH);
    void EndRangeMeasure();
    bool HasMeasuredRange() const { return m_hasMeasure; }
    double GetMeasuredDistanceMeters() const { return m_measuredMeters; }

    CString GetHoverText() const { return m_hoverText; }
    void SetHoverPoint(int x, int y, int viewW, int viewH);

    bool IsCameraOutOfRange() const;

private:
    cv::Mat m_mapBgr;
    GeoBounds m_bounds;

    double m_cameraLat = 0.0;
    double m_cameraLon = 0.0;
    double m_panDeg = 0.0;
    double m_tiltDeg = 0.0;
    double m_hfovDeg = 10.0;

    double m_centerX = 0.0; // image pixel center
    double m_centerY = 0.0;
    double m_zoom = 1.0;
    double m_minZoom = 0.2;
    double m_maxZoom = 20.0;

    bool m_rangeMode = false;
    bool m_clickMoveEnabled = true;

    bool m_hasMeasure = false;
    double m_measuredMeters = 0.0;
    double m_measureLat1 = 0.0;
    double m_measureLon1 = 0.0;
    double m_measureLat2 = 0.0;
    double m_measureLon2 = 0.0;

    CString m_hoverText;
    std::vector<Marker> m_markers;

private:
    double GetFitMinZoom(int viewW, int viewH) const;
    void ClampCenter(int viewW, int viewH);

private:
    bool LatLonToImage(double lat, double lon, double& imgX, double& imgY) const;
    bool ImageToLatLon(double imgX, double imgY, double& lat, double& lon) const;

    void ClampCenter();
    void DrawCamera(cv::Mat& dst, int viewW, int viewH) const;
    void DrawMarkers(cv::Mat& dst, int viewW, int viewH) const;
    void DrawMeasure(cv::Mat& dst, int viewW, int viewH) const;
    void DrawHud(cv::Mat& dst) const;

    static double NormalizeDeg(double deg);
    static double HaversineMeters(double lat1, double lon1, double lat2, double lon2);
};