#pragma once
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <boost/asio.hpp>
#include "room.h"

class RoomManager : public std::enable_shared_from_this<RoomManager>
{
    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket &_udpSocket;

    int _roomIndex;
    std::map<int, std::shared_ptr<Room>> _rooms;
    std::mutex _roomsMtx;

    std::map<int, std::weak_ptr<ClientSocket>> _listingClients;
    std::mutex _listingClientMtx;

    std::map<int, std::pair<int, std::string>> _roomListCache; // <list index, room index>
    std::mutex _roomListCacheMtx;

private:
    void SendCurrentRoomList(std::shared_ptr<ClientSocket> client);
    void CreateRoom(std::shared_ptr<ClientSocket> client, const std::string &name);
    void EnterRoom(std::shared_ptr<ClientSocket> client, int roomIndex);
    void LeaveRoom(std::shared_ptr<ClientSocket> client);

public:
    RoomManager(boost::asio::io_context &io,
                boost::asio::ip::udp::socket &socket);

    bool Enter(int index, std::weak_ptr<ClientSocket> client);
    void DisconnectedClient(std::weak_ptr<ClientSocket> client);
    std::weak_ptr<Room> GetRoom(int index);
    void ForwardUDPData(char* data, int length);

    void PrintStatus();
};