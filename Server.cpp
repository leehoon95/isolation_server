#include "Server.h"
#include <iostream>
#include "protoc/login_message.pb.h"
#include "protoc/error_message.pb.h"
#include "util.h"

using namespace boost;

Server::Server(
    asio::io_context &io)
    : _io(io),
      _udpSocket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 51022))
{
    _udpRecvBuffer = std::shared_ptr<char[]>(
        new (std::nothrow) char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    ASSERT(_udpRecvBuffer != nullptr, "Server. Failed to allocate UDP receive buffer");

    try
    {
        _rm = std::make_shared<RoomManager>(io, _udpSocket);
    }
    catch (const std::bad_alloc &e)
    {
        ASSERT(false, "Server. Failed to allocate RoomManager");
    }
}

void Server::ReceiveUDP()
{
    _udpSocket.async_receive_from(
        asio::buffer(_udpRecvBuffer.get(), static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE)),
        _remoteEndpoint,
        [this](system::error_code ec, std::size_t length)
        {
            if (!ec && length > 0)
            {
                // for (int i = 0; i < length; ++i) {
                //     printf("%0X ", _udpRecvBuffer[i]);
                // }

                // std::cout << std::endl;

                if (memcmp(_udpRecvBuffer.get(), "prot", 4) == 0)
                {
                    int type = *(int32_t *)(&_udpRecvBuffer[4]);
                    int totalDataLength = *(int32_t *)(&_udpRecvBuffer[8]);

                    if (totalDataLength != length)
                    {
                        std::cout << std::format("UDP prot data is insufficient. buffer data length: {}, packet data length: {}\n",
                                                 length, totalDataLength);
                    }
                    else
                    {
                        _rm->ForwardUDPData(&_udpRecvBuffer[12], length);
                    }
                }

                ReceiveUDP();
            }
            else
            {
                print_boost_system_error("UDP async_receive_from.error.", ec);
                std::cout << std::format("UDP error: {}\n", ec.what());
            }
        });
}

void Server::RemoveClient(int index)
{
    std::scoped_lock sl{_connMtx};

    _connectedClients.erase(index);
}

// bool Server::TryLogin(std::shared_ptr<ClientSocket> client, std::string &reason)
// {
//     return _rm->Login(std::weak_ptr<ClientSocket>(client), reason);
// }

void Server::Stop()
{
    std::scoped_lock sl{_connMtx};

    for (auto &c : _connectedClients)
    {
        c.second->Stop();
    }

    _connectedClients.clear();
}

void Server::AddClient(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_connMtx};
    _clientIndex++;
    _connectedClients[_clientIndex] = client;
    client->Init(_clientIndex);

    std::weak_ptr wclient{client};
    auto self = shared_from_this();

    // io_context.stop()이 Server 소멸보다 먼저 호출될 것을 보장할 것
    client->SetPacketHandler(
        LM_Type::CM_REQUEST_LOGIN,
        [self, wclient](char *serializedData, int length)
        {
            if (auto c = wclient.lock())
            {
                self->HandleRequestLogin(c, serializedData, length);
            }
        });

    client->SetDisconnectHandler(
        EM_Type::EM_DISCONNECTED,
        [self, wclient](system::error_code &ec)
        {
            if (auto c = wclient.lock())
            {
                int index = c->GetIndex();

                c->Stop();
                self->RemoveClient(index);
            }
        });
}

void Server::HandleRequestLogin(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    LM_RequestLogin msg;
    // std::cout << std::format("c use_count: {}\n", client.use_count());
    LM_LoginResult lr;

    if (msg.ParseFromArray(serializedData, length))
    {
        std::string nickname{msg.nickname()};

        client->SetNickname(nickname);

        {
            std::string reason;
            bool res = _rm->Login(client, reason);

            if (res)
            {
                lr.set_token(client->GetToken());
                lr.set_reason("ok");
            }
            else
            {
                lr.set_token(0);
                lr.set_reason(std::move(reason));
            }
        }
    }
    else
    {
        lr.set_token(0);
        lr.set_reason("CTM_REQUEST_LOGIN parsing error.");
    }

    std::string lrString{lr.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LM_Type::SM_RESPONSE_LOGIN),
        static_cast<int>(lrString.length()) + 12);
    t.insert(t.end(), lrString.begin(), lrString.end());

    client->PostWrite(t);
}

void Server::StartUDPReceive()
{
    for (int i = 0; i < 8; ++i)
    {
        std::cout << std::format("Receive UDP data {}", i);
        ReceiveUDP();
    }
}

void Server::PrintStatus()
{
    std::scoped_lock sl{_connMtx};

    std::cout << "--- connected clients ---\n";
    for (auto &pair : _connectedClients)
    {
        std::cout << std::format("{}: ---\n", pair.first);
    }

    std::cout << "-------------------------\n\n";

    _rm->PrintStatus();
}