#pragma once
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include "session.h"


class LobbyManager : public std::enable_shared_from_this<LobbyManager>
{
    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket &_udpSocket;

    int _sessionIndexCounter;
    std::map<int, std::shared_ptr<Session>> _emptySessions; // session token, session
    std::mutex _emptySessionsMtx;

    std::map<int, std::shared_ptr<Session>> _sessions; // session token, session
    std::mutex _sessionsMtx;
    boost::asio::steady_timer _timer;
    

    std::map<uint64_t, std::shared_ptr<ClientSocket>> _loginedClients; // token, client
    std::mutex _loginedClientMtx;

    std::string _serializedRSLCache;
    std::mutex _RSLCacheMtx;

private:
    void SendCurrentActivedSessionList(std::shared_ptr<ClientSocket> client);
    int CreateSession(
        std::shared_ptr<ClientSocket> client,
        std::string_view name,
        std::string_view password,
        std::string &reason);
    void ActivateSession(int sessionIndex);

    void HandleRequestSessionCreate(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestSessionEnter(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestSessionLeave(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestLogout(std::shared_ptr<ClientSocket> client, char* serializedData, int length);

    void DisconnectedClient(std::shared_ptr<ClientSocket> client);
    void SetClientHandler(std::shared_ptr<ClientSocket> client);

    void RewriteSessionListCache();

public:
    LobbyManager(boost::asio::io_context &io,
                boost::asio::ip::udp::socket &socket);

    bool RequestEnterLobby(std::shared_ptr<ClientSocket> client, std::string &reason);

    //std::weak_ptr<Room> GetRoom(int index);
    void ForwardUDPData(char* data, int length);
    void StartRefreshSessionCache();
    void PrintStatus();
};