#include "pch.h"
#include "RtspBuild.h"

CString CRtspBuilder::MakeRtsp(const DeviceCamInfo& cam, const CString& path)
{
    CString url;
    url.Format(L"rtsp://%s:%s@%s:%d/%s",
        cam.id.GetString(),
        cam.pw.GetString(),
        cam.ip.GetString(),
        554,
        path.GetString());
    return url;
}

CString CRtspBuilder::BuildColorRtsp(const DeviceProfile& profile)
{
    CString maker(profile.color.maker);

    if (maker.CompareNoCase(L"Bosch") == 0)
        return MakeRtsp(profile.color, L"rtsp_tunnel");

    if (maker.CompareNoCase(L"한화비전") == 0)
        return MakeRtsp(profile.color, L"profile2/media.smp");

    return MakeRtsp(profile.color, L"profile2/media.smp");
}

CString CRtspBuilder::BuildThermalRtsp(const DeviceProfile& profile)
{
    CString maker(profile.thermal.maker);

    if (maker.CompareNoCase(L"FLIR") == 0)
        return MakeRtsp(profile.thermal, L"stream2");

    if (maker.CompareNoCase(L"I3 System") == 0)
        return MakeRtsp(profile.thermal, L"cam0_0");

    return MakeRtsp(profile.thermal, L"rtsp_tunnel");
}