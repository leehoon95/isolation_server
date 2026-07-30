#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <boost/asio.hpp>

class IClient
{
public:
    virtual bool Init() = 0;
    virtual void Stop() = 0;
    virtual bool PostWrite(std::vector<char> &data) = 0;
    virtual void SetPacketHandler(int type, std::function<void(char *, int)> handler) = 0;
    virtual void SetErrorHandler(int type, std::function<void(boost::system::error_code&)> handler) = 0;
    virtual void RemovePacketHandler(int type) = 0;;
    virtual void RemoveDisconnectHandler(int type) = 0;
    virtual void ClearPacketHandler() = 0;;
    virtual void ClearDisconnectHandler() = 0;;
    virtual uint64_t GetToken() = 0;
    virtual void SetLoginKey(std::string& key) = 0;
    virtual std::string GetLoginKey() = 0;

    virtual ~IClient() = default;
};