#pragma once
#include "../src/clientInterface.h"
#include <gmock/gmock.h>
#include <vector>


class MockClientSocket : public IClient
{
public:
    MOCK_METHOD(bool, Init, (), (override));
    MOCK_METHOD(void, Stop, (), (override));
    MOCK_METHOD(bool, PostWrite, (std::vector<char> &data), (override));
    MOCK_METHOD(void, SetPacketHandler, (int type, std::function<void(char *, int)> handler), (override));
    MOCK_METHOD(void, SetErrorHandler, (int type, std::function<void(boost::system::error_code&)> handler), (override));
    MOCK_METHOD(void, RemovePacketHandler, (int type), (override));
    MOCK_METHOD(void, RemoveDisconnectHandler, (int type), (override));
    MOCK_METHOD(void, ClearPacketHandler, (), (override));
    MOCK_METHOD(void, ClearDisconnectHandler, (), (override));
    MOCK_METHOD(uint64_t, GetToken, (), (override));
    MOCK_METHOD(void, SetLoginKey, (std::string& key), (override));
    MOCK_METHOD(std::string, GetLoginKey, (), (override));
    virtual ~MockClientSocket() = default;
};