#pragma once
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <boost/asio.hpp>
#include "room.h"
#include "tokenPool.h"

class LobbyManager : public std::enable_shared_from_this<LobbyManager>
{
    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket &_udpSocket;

    int _roomIndex;
    std::map<int, std::shared_ptr<Room>> _rooms;
    std::mutex _roomsMtx;

    std::map<uint64_t, std::shared_ptr<ClientSocket>> _loginedClients;
    std::mutex _loginedClientMtx;

    std::map<int, std::pair<int, std::string>> _roomListCache; // <list index, <room index, room name>>
    std::mutex _roomListCacheMtx;

    TokenPool64 _tokenPool;

private:
    void SendCurrentRoomList(std::shared_ptr<ClientSocket> client);
    int CreateRoom(std::shared_ptr<ClientSocket> client, const std::string &name, std::string &reason);
    bool EnterRoom(std::shared_ptr<ClientSocket> client, int roomIndex, std::string &reason);
    void LeaveRoom(std::shared_ptr<ClientSocket> client, int roomIndex);

    void HandleRequestCreateRoom(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestEnterToRoom(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestLeaveFromRoom(std::shared_ptr<ClientSocket> client, char* serializedData, int length);
    void HandleRequestLogout(std::shared_ptr<ClientSocket> client, char* serializedData, int length);

        void DisconnectedClient(std::shared_ptr<ClientSocket> client);
    //void LeaveRoom(std::shared_ptr<ClientSocket> client);

public:
    LobbyManager(boost::asio::io_context &io,
                boost::asio::ip::udp::socket &socket);

    bool Login(std::shared_ptr<ClientSocket> wc, std::string &reason);

    std::weak_ptr<Room> GetRoom(int index);
    void ForwardUDPData(char* data, int length);

    void PrintStatus();
};