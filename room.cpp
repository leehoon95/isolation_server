#include "room.h"
#include <cassert>
#include <iostream>
#include "util.h"

using namespace boost;

static_assert(std::is_trivially_copyable<ObjectTransform>::value, "memcpy unsafe for non-trivially-copyable types");

Room::Room(boost::asio::io_context &io, asio::ip::udp::socket &socket)
    : _roomIndex(100),
      _io(io),
      _timer(io, std::chrono::milliseconds(500)),
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

    std::scoped_lock sl{_mtxSCP};
    std::string serialized{_scp.SerializePartialAsString()};

    if (!_clientUDPEndpoint.empty())
    {
        for (auto &ep : _clientUDPEndpoint)
        {
            std::scoped_lock sl{_mtxSendQueue};

            std::vector<char> buffer;
            append_prot_packet(buffer, static_cast<int>(LOGIN_RESULT), static_cast<int>(serialized.length()));
            buffer.insert(buffer.end(), serialized.begin(), serialized.end());

            _sendQueue.push(std::move(buffer));

            SendUDPData(ep.second);
        }
        _timer.expires_after(std::chrono::milliseconds(500));
    }
    else {
        std::cout << "Any clients didn't send udp data.\n";
        _timer.expires_after(std::chrono::milliseconds(1000));
    }

    _timer.async_wait([this](const system::error_code &ec)
                      {
        if (!ec) {
            std::cout << "tick!\n";
            StartTimer();
        }
        else
            print_boost_system_error("Room timer error", ec); });
}

void Room::SendUDPData(asio::ip::udp::endpoint remoteEndpoint)
{
    _udpSocket.async_send_to(
        asio::buffer(_sendQueue.back()),
        remoteEndpoint,
        [this](system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                std::cout << std::format("UDP sent {}\n", length);
            }
            else
            {
                std::cout << std::format("UDP send error: {}\n", ec.what());
            }

            std::scoped_lock sl{_mtxSendQueue};
            _sendQueue.pop();
        });
}

void Room::EnterRoom(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_mtxClient};
    for (auto &var : _clients)
    {
        if (var->GetIndex() == client->GetIndex())
            return;
    }

    _clients.push_back(client);

    std::cout << "pscp count " << _scp.transfoms_size() << std::endl;
}

void Room::ExitRoom(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_mtxClient, _mtxSCP, _mtxClientUDPEndpoint};

    bool found = false;
    auto iter = _clients.begin();
    int clientIndex = client->GetIndex();

    for (; iter != _clients.end(); ++iter)
    {
        if ((*iter)->GetIndex() == clientIndex)
            break;
    }

    _clients.erase(iter);

    auto &t = _scp.transfoms();

    for (int i = 0; i < t.size();)
    {
        if (_scp.transfoms(i).clientindex() == clientIndex)
        {
            _scp.mutable_transfoms()->DeleteSubrange(i, 1);
            break;
        }
        else
        {
            ++i;
        }
    }

    _clientUDPEndpoint.erase(clientIndex);
}

void Room::ReportClientTransform(PROTO_ObjectTransform *ot, asio::ip::udp::endpoint sender)
{
    std::scoped_lock sl{_mtxSCP, _mtxClientUDPEndpoint};
    int clientIndex = ot->clientindex();

    _clientUDPEndpoint[clientIndex] = sender;

    auto &t = _scp.transfoms();

    bool found = false;

    for (int i = 0; i < t.size();)
    {
        if (_scp.transfoms(i).clientindex() == clientIndex)
        {
            std::memcpy(_scp.mutable_transfoms(i), ot, sizeof(PROTO_ObjectTransform));
            found = true;

            return;
        }
    }

    if (!found)
    {
        auto newt = _scp.add_transfoms();

        std::memcpy(newt, ot, sizeof(PROTO_ObjectTransform));
        std::cout << "New transforms is added.\n";
    }
}

void Room::PrintStatus()
{
    std::scoped_lock sl{_mtxSendQueue, _mtxClient};

    std::cout << "--- Room status ---\n";

    std::cout << "Room index : " << _roomIndex << std::endl;

    for (auto &var : _clients)
    {
        std::cout << std::format("{} : {}\n", var->GetIndex(), var->GetNickname());
    }

    std::cout << "-------------------\n\n";
}

Room::~Room()
{
}