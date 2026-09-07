#pragma once
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>
#include "redisInterface.h"
#include "clientInterface.h"

class Server : public std::enable_shared_from_this<Server>
{
    boost::asio::io_context &_io;
    std::shared_ptr<IRedis> _rs;

    std::map<uint64_t, std::shared_ptr<IClient>> _connectedClients;
    std::mutex _connMtx;

private:
    void RemoveClient(uint64_t token);
    void HandleRequestCreationAccount(
        std::shared_ptr<IClient> client,
        char *serializedData, int length);
    void HandleRequestLogin(
        std::shared_ptr<IClient> client,
        char *serializedData, int length);
    void HandleRequestPlayerData(
        std::shared_ptr<IClient> client,
        char *serializedData, int length);
    void HandleRequestLogout(
        std::shared_ptr<IClient> client,
        char *serializedData, int length);

    void LogoutClient(std::shared_ptr<IClient> client);

public:
    explicit Server(
        boost::asio::io_context &io,
        std::shared_ptr<IRedis> rs);
    void Stop();
    int AddClient(std::shared_ptr<IClient> client);
    void PrintStatus();
};
