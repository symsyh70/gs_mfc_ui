#pragma once
#include "DeviceProfile.h"

class CRtspBuilder
{
public:
    static CString BuildColorRtsp(const DeviceProfile& profile);
    static CString BuildThermalRtsp(const DeviceProfile& profile);

private:
    static CString MakeRtsp(const DeviceCamInfo& cam, const CString& path);
};