#include "room.h"
#include <cassert>
#include <iostream>
#include "util.h"

using namespace boost;

static_assert(std::is_trivially_copyable<ObjectTransform>::value, "memcpy unsafe for non-trivially-copyable types");

Room::Room(boost::asio::io_context &io, asio::ip::udp::socket &socket)
    : _roomIndex(100),
      _io(io),
      _timer(io),
      _udpSocket(socket)
{
    _udpRecvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    StartTimer();
}

void Room::StartTimer()
{
    /*
        mtx lock...
        async send data...
    */
   
    // std::cout << std::format("scp transforms_size: {}\n", _scp.transfoms_size());
    if (_scp.transfoms_size() > 0)
    // if (_transforms.size() > 0)
    {
        std::scoped_lock sl{_mtxSCP};
        // PROTO_SyncCharacterPhysics scp;

        // for (auto& tr : _transforms) {
        //     auto newtr = scp.add_transfoms();

        //     //std::memcpy(newtr, tr.second.get(), sizeof(PROTO_ObjectTransform));
        //     tr.second
        //     newtr->set_clientindex()
        // }
      
        std::string serialized{_scp.SerializePartialAsString()};

        if (!_clientUDPEndpoint.empty())
        {
            for (auto &ep : _clientUDPEndpoint)
            {
                //std::scoped_lock sl{_mtxSendQueue};

                std::vector<char> buffer;
                append_prot_packet(
                    buffer,
                    static_cast<int>(SYNC_CHARACTER_PHYSICS),
                    static_cast<int>(serialized.length()) + 12);
                buffer.insert(buffer.end(), serialized.begin(), serialized.end());

                //_sendQueue.push(std::move(buffer));

                SendUDPData(
                    ep.second, 
                    std::make_shared<std::vector<char>>(std::move(buffer))
                );
            }
        }
    }

    _timer.expires_after(std::chrono::milliseconds(
        static_cast<int>(BroadcastingInterval::INTERVAL)));
    _timer.async_wait([this](const system::error_code &ec)
                      {
        if (!ec) {
            //std::cout << "tick!\n";
            StartTimer();
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

void Room::EnterRoom(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_mtxClient};
    for (auto &var : _clients)
    {
        if (_clients.find(client->GetIndex()) != _clients.end())
            return;
    }

    _clients[client->GetIndex()] = client;

    std::cout << std::format("client {}: {} is entered.\n",
                             client->GetIndex(), client->GetNickname());
}

void Room::ExitRoom(const int index)
{
    std::scoped_lock sl{_mtxClient, _mtxSCP, _mtxClientUDPEndpoint, _mtxTransforms};
    std::cout << std::format("Client({}) exit room. {}\n", index, _clients[index]->GetNickname());
    bool found = false;

    if (_clients.find(index) != _clients.end())
        _clients.erase(index);

    int trsize = _scp.transfoms_size();
    for (int i = 0; i < trsize; ++i)
    {
        if (_scp.transfoms(i).clientindex() == index)
        {
            _scp.mutable_transfoms()->DeleteSubrange(i, 1);
            break;
        }
        else
        {
            ++i;
        }
    }

    std::cout << std::format("scp count: {}", _scp.transfoms_size());
    _clientUDPEndpoint.erase(index);
}

void Room::ReportClientTransform(PROTO_ObjectTransform *ot, asio::ip::udp::endpoint sender)
{
    std::scoped_lock sl{_mtxSCP, _mtxClientUDPEndpoint, _mtxTransforms};

    int clientIndex = ot->clientindex();

    if (_clients.find(clientIndex) == _clients.end())
    {
        std::cout << std::format("ObjectTransform.clientIndex is not in _clients(index: {}\n", clientIndex);
        return;
    }

    // std::cout << std::format("client transfom reported: {}\n", clientIndex);

    _clientUDPEndpoint[clientIndex] = sender;

    // _transforms[clientIndex] = std::make_shared<PROTO_ObjectTransform>();
    // _transforms[clientIndex]->CopyFrom(*ot);

    bool found = false;
    int trsize = _scp.transfoms_size();

    for (int i = 0; i < trsize; ++i)
    {
        if (_scp.transfoms(i).clientindex() == clientIndex)
        {
            // std::memcpy(_scp.mutable_transfoms(i), ot, sizeof(PROTO_ObjectTransform));
            auto tr = _scp.mutable_transfoms(i);
            tr->set_clientindex(ot->clientindex());
            tr->set_x(ot->x());
            tr->set_y(ot->y());
            tr->set_z(ot->z());
            tr->set_r(ot->r());

            found = true;
            // std::cout << std::format("client transforms is set. {}\n", _scp.transfoms(i).clientindex());

            return;
        }
    }

    if (!found)
    {
        auto tr = _scp.add_transfoms();
        tr->set_clientindex(ot->clientindex());
        tr->set_x(ot->x());
        tr->set_y(ot->y());
        tr->set_z(ot->z());
        tr->set_r(ot->r());
        // std::memcpy(newt, ot, sizeof(PROTO_ObjectTransform));
        std::cout << std::format("New transforms is added. {}\n", tr->clientindex());
    }
}

void Room::PrintStatus()
{
    std::scoped_lock sl{_mtxClient};

    std::cout << "--- Room status ---\n";

    std::cout << "Room index : " << _roomIndex << std::endl;

    for (auto &var : _clients)
    {
        std::cout << std::format("{} : {}\n", var.second->GetIndex(), var.second->GetNickname());
    }

    std::cout << "-------------------\n\n";
}

Room::~Room()
{
}