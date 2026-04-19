#pragma once
#include "DeviceProfile.h"

#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

class CMongoManager
{
public:
    CMongoManager();
    ~CMongoManager();

    bool Connect(const std::string& uri);
    bool Ping();

    bool GetDeviceList(std::vector<DeviceListItem>& outList);
    bool LoadDeviceProfile(const CString& unitId, DeviceProfile& outProfile);

private:
    static bool GetStringField(const bsoncxx::document::view& v, const char* key, std::string& out);
    static void ReadCamInfo(const bsoncxx::document::view& v, DeviceCamInfo& cam);

private:
    std::unique_ptr<mongocxx::client> m_client;
    static mongocxx::instance s_instance;
};