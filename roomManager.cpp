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

bool RoomManager::Login(std::shared_ptr<ClientSocket> client, std::string &reason)
{
    if (!client)
    {
        reason = "RoomManager::Login: Client is not valid.";
        return false;
    }

    std::string nickname = client->GetNickname();

    if (nickname.length() < 3)
    {
        reason = "RoomManager::Login: Client nickname is not valid.";
        return false;
    }

    std::scoped_lock<std::mutex> sl{_loginedClientMtx};

    // 닉네임 중복검사는 하지 않음 25.07.16
    // if (_loginedClients.find(nickname) != _loginedClients.end())
    // {
    //     return "RoomManager::Login: Client is logined aleady.";
    // }

    client->ClearPacketHandler();
    client->ClearDisconnectHandler();

    auto token = _tokenPool.allocate();

    _loginedClients[token] = client;
    client->SetToken(token);

    std::weak_ptr<ClientSocket> wc{client};
    std::weak_ptr<RoomManager> wrm{shared_from_this()};

    client->SetPacketHandler(
        RM_Type::CM_REQUEST_ROOM_LIST,
        [wrm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto rm = wrm.lock())
                {
                    rm->SendCurrentRoomList(c);
                }
            }
        });

    client->SetPacketHandler(
        RM_Type::CM_REQUEST_CREATE_ROOM,
        [wrm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto rm = wrm.lock())
                {
                    rm->HandleRequestCreateRoom(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        RM_Type::CM_REQUEST_ROOM_ENTER,
        [wrm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto rm = wrm.lock())
                {
                    rm->HandleRequestEnterToRoom(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        RM_Type::CM_REQUEST_ROOM_LEAVE,
        [wrm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto rm = wrm.lock())
                {
                    rm->HandleRequestLeaveFromRoom(c, serializedData, length);
                }
            }
        });

    client->SetDisconnectHandler(
        RM_Type::RM_DISCONNECTED,
        [wrm, wc](system::error_code &ec)
        {
            if (auto c = wc.lock())
            {
                if (auto rm = wrm.lock())
                {
                    rm->DisconnectedClient(c);
                }
            }
        });

    reason = "ok";

    return true;
}

void RoomManager::DisconnectedClient(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_loginedClientMtx};

    client->Stop();
    auto token = client->GetToken();

    _loginedClients.erase(token);
    _tokenPool.release(token);
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

    RM_ResultRoomList rrl;

    if (_roomListCache.empty())
    {
        rrl.set_count(0);
    }
    else
    {
        rrl.set_count(static_cast<int>(_roomListCache.size()));

        for (const auto &entry : _roomListCache)
        {
            RM_RoomSmallInfo *rsi = rrl.add_list();
            rsi->set_roomindex(entry.second.first);
            rsi->set_roomname(entry.second.second);
        }
    }

    std::string rrlString{rrl.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(RM_Type::SM_RESPONSE_ROOM_LIST),
        static_cast<int>(rrlString.length()) + 12);
    t.insert(t.end(), rrlString.begin(), rrlString.end());

    client->PostWrite(t);
}

int RoomManager::CreateRoom(std::shared_ptr<ClientSocket> client, const std::string &name, std::string &reason)
{
    std::scoped_lock<std::mutex> sl{_roomsMtx};

    if (name.length() < 4)
    {
        reason = "The room name is too short.";

        return -1;
    }
    else
    {
        ++_roomIndex;
        auto room = std::make_shared<Room>(_io, _udpSocket);

        room->SetIndex(_roomIndex);
        room->SetName(name);

        _rooms[_roomIndex] = room;

        std::string erReason;
        bool res = room->EnterRoom(client, erReason);

        if (!res)
        {
            reason = std::move(erReason);
            _rooms.erase(_roomIndex);
            
            return -1;
        }

        return _roomIndex;
    }
}

bool RoomManager::EnterRoom(std::shared_ptr<ClientSocket> client, int roomIndex, std::string &reason)
{
    std::scoped_lock<std::mutex> sl{_roomsMtx};

    if (_rooms.find(roomIndex) == _rooms.end())
    {
        reason = "Room does not exist.";

        return false; // Room does not exist
    }

    bool res = _rooms[roomIndex]->EnterRoom(client, reason);

    return res;
}

void RoomManager::LeaveRoom(std::shared_ptr<ClientSocket> client, int roomIndex)
{
}

void RoomManager::HandleRequestCreateRoom(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    RM_RequestCreateRoom rcr;
    RM_ResultCreateRoom response;

    if (rcr.ParseFromArray(serializedData, length))
    {
        std::string roomName{rcr.roomname()};

        std::string reason;
        int res = CreateRoom(client, roomName, reason);

        if (res > 0)
        {
            response.set_roomindex(res);
            response.set_reason("ok");
        }
        else
        {
            response.set_roomindex(-1);
            response.set_reason(reason);
        }
    }
    else
    {
        response.set_roomindex(-1);
        response.set_reason("Parsing message error.");
    }

    std::string rcrString{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(RM_Type::SM_RESPONSE_CREATE_ROOM),
        static_cast<int>(rcrString.length()) + 12);

    client->PostWrite(t);
}

void RoomManager::HandleRequestEnterToRoom(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    RM_RequestEnterRoom rer;
    RM_ResultEnterRoom response;

    if (rer.ParseFromArray(serializedData, length))
    {
        int roomIndex = rer.roomindex();

        std::string reason;
        bool res = EnterRoom(client, roomIndex, reason);

        response.set_result(res);

        if (res)
        {
            response.set_reason("ok");
        }
        else
        {
            response.set_reason(reason);
        }
    }
    else
    {
        response.set_result(false);
        response.set_reason("Parsing message error");
    }

    std::string rcrString{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(RM_Type::SM_RESPONSE_CREATE_ROOM),
        static_cast<int>(rcrString.length()) + 12);

    client->PostWrite(t);
}

void RoomManager::HandleRequestLeaveFromRoom(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
}

void RoomManager::HandleRequestLogout(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
}

void RoomManager::ForwardUDPData(char *data, int length)
{
}