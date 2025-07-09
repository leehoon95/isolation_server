#include "roomManager.h"

using namespace boost;

RoomManager::RoomManager(asio::io_context &io, asio::ip::udp::socket &socket)
    : _io(io),
      _udpSocket(socket)
{
}

void RoomManager::LoginedClient(int index, std::weak_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_lcMtx};
    _listingClients[index] = client;
}

void RoomManager::DisconnectedClient(std::weak_ptr<ClientSocket> client)
{
}

std::weak_ptr<Room> RoomManager::GetRoom(int roomIndex)
{
    std::scoped_lock<std::mutex> sl{_roomsMtx};
    if (_rooms.find(roomIndex) == _rooms.end())
        return std::weak_ptr<Room>();

    return _rooms[roomIndex];
}

void RoomManager::PrintStatus()
{
}