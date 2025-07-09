#pragma once
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <boost/asio.hpp>
#include "room.h"

class RoomManager
{
    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket &_udpSocket;

    std::map<int, std::shared_ptr<Room>> _rooms;
    std::mutex _roomsMtx;

    std::map<int, std::weak_ptr<ClientSocket>> _listingClients;
    std::mutex _lcMtx;

public:
    RoomManager(boost::asio::io_context &io,
                boost::asio::ip::udp::socket &socket);
    
    void LoginedClient(int index, std::weak_ptr<ClientSocket> client);
    void DisconnectedClient(std::weak_ptr<ClientSocket> client);
    std::weak_ptr<Room> GetRoom(int index);
    void PrintStatus();
};