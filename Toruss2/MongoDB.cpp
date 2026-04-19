#include "pch.h"
#include "MongoDB.h"

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

CMongoManager::CMongoManager() {}
CMongoManager::~CMongoManager() {}

static std::string CStringToUtf8(const CString& str)
{
    CW2A utf8(str, CP_UTF8);
    return std::string(utf8);
}

static CString Utf8ToCString(const std::string& utf8)
{
    CA2W wide(utf8.c_str(), CP_UTF8);
    return CString(wide);
}

bool CMongoManager::Connect(const std::string& uri)
{
    try
    {
        mongocxx::uri mongoUri(uri);
        m_client = std::make_unique<mongocxx::client>(mongoUri);
        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(e.what());
        return false;
    }
}

bool CMongoManager::Ping()
{
    if (!m_client) return false;

    try
    {
        auto db = (*m_client)["admin"];
        auto cmd = make_document(kvp("ping", 1));
        db.run_command(cmd.view());
        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(e.what());
        return false;
    }
}

bool CMongoManager::GetStringField(const bsoncxx::document::view& v, const char* key, std::string& out)
{
    auto it = v.find(key);
    if (it == v.end() || it->type() != bsoncxx::type::k_string)
        return false;

    out = std::string(it->get_string().value);
    return true;
}

void CMongoManager::ReadCamInfo(const bsoncxx::document::view& v, DeviceCamInfo& cam)
{
    std::string makerA, ipA, idA, pwA;

    GetStringField(v, "maker", makerA);
    GetStringField(v, "ip", ipA);
    GetStringField(v, "id", idA);
    GetStringField(v, "pw", pwA);

    cam.maker = Utf8ToCString(makerA);
    cam.ip = Utf8ToCString(ipA);
    cam.id = Utf8ToCString(idA);
    cam.pw = Utf8ToCString(pwA);
}

bool CMongoManager::GetDeviceList(std::vector<DeviceListItem>& outList)
{
    outList.clear();
    if (!m_client) return false;

    try
    {
        auto col = (*m_client)["toruss"]["device_profile"];

        mongocxx::options::find opt;
        opt.projection(make_document(
            kvp("unitId", 1),
            kvp("siteName", 1),
            kvp("deviceModel", 1),
            kvp("_id", 0)
        ));

        auto cursor = col.find({}, opt);

        for (auto&& doc : cursor)
        {
            auto v = doc;

            std::string unitIdA, siteA, modelA;
            if (!GetStringField(v, "unitId", unitIdA))
                continue;

            GetStringField(v, "siteName", siteA);
            GetStringField(v, "deviceModel", modelA);

            DeviceListItem item;
            item.unitId = Utf8ToCString(unitIdA);
            item.siteName = Utf8ToCString(siteA);
            item.deviceModel = Utf8ToCString(modelA);

            outList.push_back(item);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(e.what());
        return false;
    }
}
bool CMongoManager::LoadDeviceProfile(const CString& unitId, DeviceProfile& outProfile)
{
    if (!m_client)
        return false;

    try
    {
        auto col = (*m_client)["toruss"]["device_profile"];
        std::string unitIdA = CStringToUtf8(unitId);

        auto filter = make_document(kvp("unitId", unitIdA));
        auto result = col.find_one(filter.view());
        if (!result)
            return false;

        auto v = result->view();

        outProfile.unitId = unitId;

        std::string siteA, modelA, ccbIpA, ccbPortA;

        GetStringField(v, "siteName", siteA);
        GetStringField(v, "deviceModel", modelA);
        GetStringField(v, "ccbIp", ccbIpA);
        GetStringField(v, "ccbPort", ccbPortA);

        outProfile.siteName = Utf8ToCString(siteA);
        outProfile.deviceModel = Utf8ToCString(modelA);
        outProfile.ccbip = Utf8ToCString(ccbIpA);
        outProfile.ccbport = Utf8ToCString(ccbPortA);

        auto itColor = v.find("color");
        if (itColor != v.end() && itColor->type() == bsoncxx::type::k_document)
            ReadCamInfo(itColor->get_document().view(), outProfile.color);

        auto itThermal = v.find("thermal");
        if (itThermal != v.end() && itThermal->type() == bsoncxx::type::k_document)
            ReadCamInfo(itThermal->get_document().view(), outProfile.thermal);

        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(e.what());
        return false;
    }
}