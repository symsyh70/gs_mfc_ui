#pragma once
#include <string>
#include <vector>
#include <afxstr.h>

struct DeviceCamInfo
{
    CString maker;
    CString ip;
    CString id;
    CString pw;
};

struct DeviceProfile
{
    CString unitId;
    CString siteName;      // 설명
    CString deviceModel;   // 장비 모델
    CString ccbip;
    CString ccbport;

    DeviceCamInfo color;
    DeviceCamInfo thermal;
};

struct DeviceListItem
{
    CString unitId;
    CString siteName;
    CString deviceModel;
};