#include "roomManager.h"
#include "util.h"
#include "protoc/room_message.pb.h"

using namespace boost;

RoomManager::RoomManager(asio::io_context &io, asio::ip::udp::socket &socket)
    : _io(io),
      _udpSocket(socket),
      _roomIndex(0)
{
}

bool RoomManager::Enter(int index, std::weak_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_listingClientMtx};

    if (_listingClients.find(index) != _listingClients.end())
    {
        // Client is already listed
        return false;
    }

    _listingClients[index] = client;

    if (auto sc = client.lock())
    {
        std::weak_ptr<RoomManager> wrm{shared_from_this()};

        sc->SetPacketHandler(
            PRM_Type::CTM_REQUEST_ROOM_LIST,
            [wrm, client](char *serializedData, int length)
            {
                if (auto c = client.lock())
                {
                    if (auto rm = wrm.lock())
                    {
                        rm->SendCurrentRoomList(c);
                    }
                }
            });

        sc->SetPacketHandler(
            PRM_Type::CTM_REQUEST_CREATE_ROOM,
            [wrm, client](char *serializedData, int length)
            {
                if (auto c = client.lock())
                {
                    if (auto rm = wrm.lock())
                    {
                        PRM_RequestCreateRoom rcr;
                        rcr.ParseFromArray(serializedData, length);

                        std::string roomName{rcr.roomname()};

                        rm->CreateRoom(c, roomName);
                    }
                }
            });

        sc->SetPacketHandler(
            PRM_Type::CTM_ROOM_ENTER,
            [wrm, client](char *serializedData, int length)
            {
                if (auto c = client.lock())
                {
                    if (auto rm = wrm.lock())
                    {
                        rm->Enter(c->GetIndex(), std::weak_ptr<ClientSocket>(c));
                    }
                }
            });

        sc->SetPacketHandler(
            PRM_Type::CTM_ROOM_ENTER,
            [wrm, client](char *serializedData, int length)
            {
                if (auto c = client.lock())
                {
                    if (auto rm = wrm.lock())
                    {
                        rm->Enter(c->GetIndex(), std::weak_ptr<ClientSocket>(c));
                    }
                }
            });
    }

    return true;
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

void RoomManager::SendCurrentRoomList(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_roomListCacheMtx};

    PRM_ResultRoomList rrl;

    if (_roomListCache.empty())
    {
        rrl.set_count(0);
    }
    else
    {
        rrl.set_count(static_cast<int>(_roomListCache.size()));

        for (const auto &entry : _roomListCache)
        {
            PRM_RoomSmallInfo *rsi = rrl.add_list();
            rsi->set_roomindex(entry.second.first);
            rsi->set_roomname(entry.second.second);
        }
    }

    std::string rrlString{rrl.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(PRM_Type::STM_RESULT_ROOM_LIST),
        static_cast<int>(rrlString.length()) + 12);
    t.insert(t.end(), rrlString.begin(), rrlString.end());

    client->PostWrite(t);
}

void RoomManager::CreateRoom(std::shared_ptr<ClientSocket> client, const std::string &name)
{
    std::scoped_lock<std::mutex> sl{_roomsMtx};

    PRM_ResultCreateRoom rcr;

    if (name.length() < 1)
    {
        rcr.set_roomindex(-1);
        rcr.set_reason("The room name is too short.");
    }
    else
    {
        ++_roomIndex;
        auto room = std::make_shared<Room>(_io, _udpSocket);

        room->SetIndex(_roomIndex);
        room->SetName(name);

        _rooms[_roomIndex] = room;

        room->EnterRoom(client);

        rcr.set_roomindex(_roomIndex);
        rcr.set_reason("ok");
    }

    std::string rcrString{rcr.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(PRM_Type::STM_RESULT_CREATE_ROOM),
        static_cast<int>(rcrString.length()) + 12);

    client->PostWrite(t);
}

void RoomManager::ForwardUDPData(char *data, int length)
{
}