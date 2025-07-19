#include "room.h"
#include <iostream>
#include "util.h"
#include "protoc/room_message.pb.h"
#include "protoc/sync_message.pb.h"

using namespace boost;

static_assert(std::is_trivially_copyable<ObjectTransform>::value, "memcpy unsafe for non-trivially-copyable types");

Room::Room(boost::asio::io_context &io, asio::ip::udp::socket &socket)
    : _index(100),
      _io(io),
      _timer(io),
      _udpSocket(socket)
{
    _udpRecvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    ASSERT(_udpRecvBuffer != nullptr, "Room. Failed to allocate UDP receive buffer");

    StartSyncTimer();
}

void Room::StartSyncTimer()
{
    /*
        mtx lock...
        async send data...
    */

    // if (_scp.transfoms_size() > 0)
    // {
    //     std::scoped_lock sl{_mtxSCP};

    //     std::string serialized{_scp.SerializePartialAsString()};

    //     if (!_clientUDPEndpoint.empty())
    //     {
    //         for (auto &ep : _clientUDPEndpoint)
    //         {
    //             //std::scoped_lock sl{_mtxSendQueue};

    //             std::vector<char> buffer;
    //             append_prot_packet(
    //                 buffer,
    //                 static_cast<int>(SYNC_CHARACTER_PHYSICS),
    //                 static_cast<int>(serialized.length()) + 12);
    //             buffer.insert(buffer.end(), serialized.begin(), serialized.end());

    //             //_sendQueue.push(std::move(buffer));

    //             SendUDPData(
    //                 ep.second,
    //                 std::make_shared<std::vector<char>>(std::move(buffer))
    //             );
    //         }
    //     }
    // }

    std::scoped_lock<std::mutex> sl{_bcmMtx};

    if (_broadcastingMessage.length() > 0)
    {
        SM_BCSync_1 bcsync1;

        bcsync1.set_message(_broadcastingMessage);

        std::string serialized{bcsync1.SerializePartialAsString()};

        if (!_clients.empty())
        {
            for (auto &ep : _clients)
            {
                std::vector<char> buffer;
                append_prot_packet(
                    buffer,
                    static_cast<int>(SM_Type::SM_BC_SYNC_1),
                    static_cast<int>(serialized.length()) + 12);
                buffer.insert(buffer.end(), serialized.begin(), serialized.end());

                ep.second->PostWrite(buffer);
            }
        }
    }

    _timer.expires_after(std::chrono::milliseconds(
        static_cast<int>(BroadcastingInterval::INTERVAL)));
    _timer.async_wait([this](const system::error_code &ec)
                      {
        if (!ec) {
            //std::cout << "tick!\n";
            StartSyncTimer();
        }
        else
            print_boost_system_error("Room timer error", ec); });
}

void Room::SendUDPData(
    asio::ip::udp::endpoint remoteEndpoint,
    std::shared_ptr<std::vector<char>> buffer)
{
    _udpSocket.async_send_to(
        asio::buffer(*buffer),
        remoteEndpoint,
        [this, buffer](system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                std::cout << std::format("UDP sent {}\n", length);
            }
            else
            {
                std::cout << std::format("UDP send error: {}\n", ec.what());
            }

            // std::scoped_lock sl{_mtxSendQueue};
            // _sendQueue.pop();
        });
}

void Room::BroadcastMessage(std::string message)
{
    std::scoped_lock sl{_mtxClient};

    for (auto &var : _clients)
    {
        std::vector<char> t;
        append_prot_packet(
            t,
            static_cast<int>(RM_Type::SM_BC_MESSAGE),
            static_cast<int>(message.length()) + 12);
        t.insert(t.end(), message.begin(), message.end());

        var.second->PostWrite(t);
    }
}

bool Room::EnterRoom(std::shared_ptr<ClientSocket> client, std::string &reason)
{
    std::scoped_lock sl{_mtxClient};

    auto token = client->GetToken();

    if (token < 1)
    {
        ASSERT(false, "Client token is not valid.");
        reason = "Client token is not valid.";
        return false;
    }

    _clients[client->GetToken()] = client;

    std::weak_ptr<Room> wr{shared_from_this()};
    std::weak_ptr<ClientSocket> wc{client};

    client->SetPacketHandler(
        RM_Type::CM_BC_MESSAGE,
        [wr, wc](char *serializedData, int length)
        {
            if (auto room = wr.lock())
            {
                if (auto c = wc.lock())
                {
                    RM_BroadcastMessage bm;

                    if (bm.ParseFromArray(serializedData, length))
                    {
                        std::cout << std::format("Client({}:{}) broadcast message: {}\n",
                                                 c->GetIndex(), c->GetNickname(), bm.message());
                        room->BroadcastMessage(bm.message());
                    }
                }
            }
        });

    std::cout << std::format("client {}: {} is entered.\n",
                             client->GetIndex(), client->GetNickname());

    reason = "ok";

    return true;

    reason = "Client is not valid.";

    return false;
}

void Room::ExitRoom(const int index)
{
    std::scoped_lock sl{_mtxClient};

    if (_clients.find(index) == _clients.end())
        return;

    auto client = _clients[index];

    std::cout << std::format("Client({}) exit room. {}\n", index, client->GetNickname());
    client->RemovePacketHandler(RM_Type::CM_BC_MESSAGE);

    _clients.erase(index);

    bool found = false;

    // int trsize = _scp.transfoms_size();
    //  for (int i = 0; i < trsize; ++i)
    //  {
    //      if (_scp.transfoms(i).clientindex() == index)
    //      {
    //          _scp.mutable_transfoms()->DeleteSubrange(i, 1);
    //          break;
    //      }
    //      else
    //      {
    //          ++i;
    //      }
    //  }

    // std::cout << std::format("scp count: {}", _scp.transfoms_size());
    //_clientUDPEndpoint.erase(index);
}

void Room::DisconnectClient(uint64_t token)
{
    std::scoped_lock sl{_mtxClient};
    _clients.erase(token);
    //_clientUDPEndpoint.erase(token);
}

// void Room::ReportClientTransform(PM_ObjectTransform *ot, asio::ip::udp::endpoint sender)
// {
//     std::scoped_lock sl{_mtxSCP, _mtxClientUDPEndpoint, _mtxTransforms};

//     int clientIndex = ot->clientindex();

//     if (_clients.find(clientIndex) == _clients.end())
//     {
//         std::cout << std::format("ObjectTransform.clientIndex is not in _clients(index: {}\n", clientIndex);
//         return;
//     }

//     // std::cout << std::format("client transfom reported: {}\n", clientIndex);

//     _clientUDPEndpoint[clientIndex] = sender;

//     // _transforms[clientIndex] = std::make_shared<PROTO_ObjectTransform>();
//     // _transforms[clientIndex]->CopyFrom(*ot);

//     bool found = false;
//     int trsize = _scp.transfoms_size();

//     for (int i = 0; i < trsize; ++i)
//     {
//         if (_scp.transfoms(i).clientindex() == clientIndex)
//         {
//             // std::memcpy(_scp.mutable_transfoms(i), ot, sizeof(PROTO_ObjectTransform));
//             auto tr = _scp.mutable_transfoms(i);
//             tr->set_clientindex(ot->clientindex());
//             tr->set_x(ot->x());
//             tr->set_y(ot->y());
//             tr->set_z(ot->z());
//             tr->set_r(ot->r());

//             found = true;
//             // std::cout << std::format("client transforms is set. {}\n", _scp.transfoms(i).clientindex());

//             return;
//         }
//     }

//     if (!found)
//     {
//         auto tr = _scp.add_transfoms();
//         tr->set_clientindex(ot->clientindex());
//         tr->set_x(ot->x());
//         tr->set_y(ot->y());
//         tr->set_z(ot->z());
//         tr->set_r(ot->r());
//         // std::memcpy(newt, ot, sizeof(PROTO_ObjectTransform));
//         std::cout << std::format("New transforms is added. {}\n", tr->clientindex());
//     }
// }

void Room::PrintStatus()
{
    std::scoped_lock sl{_mtxClient};

    std::cout << std::format("--- Room {}/{} status ---\n", _index, _name);

    std::cout << "Room index : " << _index << std::endl;

    for (auto &var : _clients)
    {
        auto client = var.second;
        std::cout << std::format("{} : {}\n", client->GetIndex(), client->GetNickname());
    }

    std::cout << "-------------------\n\n";
}

Room::~Room()
{
}