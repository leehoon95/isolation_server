#pragma once
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>
#include "redisService.h"

class Server : public std::enable_shared_from_this<Server>
{
    enum class UDPBufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096,
        // SEND_BUFFER_SIZE = 4096
    };

    boost::asio::io_context &_io;
    std::shared_ptr<char[]> _udpRecvBuffer;
    std::unique_ptr<RedisService> _rs;

    std::map<uint64_t, std::shared_ptr<ClientSocket>> _connectedClients;
    std::mutex _connMtx;
    boost::asio::steady_timer _timer;

private:
    void RemoveClient(uint64_t token);
    void HandleRequestCreationAccount(
        std::shared_ptr<ClientSocket> client,
        char *serializedData, int length);
    void HandleRequestLogin(
        std::shared_ptr<ClientSocket> client,
        char *serializedData, int length);
    void HandleRequestPlayerData(
        std::shared_ptr<ClientSocket> client,
        char *serializedData, int length);
    void HandleRequestLogout(
        std::shared_ptr<ClientSocket> client,
        char *serializedData, int length);

    void LogoutClient(std::shared_ptr<ClientSocket> client);

public:
    explicit Server(
        boost::asio::io_context &io,
        std::unique_ptr<RedisService> rs);
    void Stop();
    int AddClient(std::shared_ptr<ClientSocket> client);
    void PrintStatus();
};
