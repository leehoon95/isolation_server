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
    std::map<int, std::shared_ptr<Session>> _sessions; // session token, session
    std::mutex _sessionsMtx;

    std::map<int, std::shared_ptr<Session>> _activatedSessions; // session token, session
    std::mutex _activatedSessionsMtx;

    

    std::map<uint64_t, std::shared_ptr<ClientSocket>> _loginedClients; // token, client
    std::mutex _loginedClientMtx;

    std::map<int, std::pair<int, std::string>> _sessionListCache; // <list index, <room index, room name>>
    std::mutex _sessionListCacheMtx;

private:
    void SendCurrentActivedSessionList(std::shared_ptr<ClientSocket> client);
    int CreateSession(
        const uint64_t hostToken,
        std::string_view name,
        std::string_view password,
        std::string &reason);
    void ActivateSession(int sessionIndex);

    void HandleRequestCreateSession(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestEnterToSession(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestLeaveFromSession(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestLogout(std::shared_ptr<ClientSocket> client, char* serializedData, int length);

    void DisconnectedClient(std::shared_ptr<ClientSocket> client);
    void SetClientHandler(std::shared_ptr<ClientSocket> client);
    //void LeaveRoom(std::shared_ptr<ClientSocket> client);

public:
    LobbyManager(boost::asio::io_context &io,
                boost::asio::ip::udp::socket &socket);

    bool RequestEnterLobby(std::shared_ptr<ClientSocket> client, std::string &reason);

    //std::weak_ptr<Room> GetRoom(int index);
    void ForwardUDPData(char* data, int length);
    
    void PrintStatus();
};