#include "Server.h"
#include <iostream>
#include "isolation_pb/login_message.pb.h"
#include "isolation_pb/error_message.pb.h"
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
        _lm = std::make_shared<LobbyManager>(io, _udpSocket);
        _lm->StartRefreshSessionCache();
    }
    catch (const std::bad_alloc &e)
    {
        ASSERT(false, "Server. Failed to allocate LobbyManager");
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
                        //_rm->ForwardUDPData(&_udpRecvBuffer[12], length);
                    }
                }

                ReceiveUDP();
            }
            else
            {
                std::cout << std::format("Server::ReceiveUDP ec what: {}", ec.message());
            }
        });
}

void Server::RemoveClient(uint64_t token)
{
    std::scoped_lock sl{_connMtx};

    _connectedClients.erase(token);
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
    _connectedClients[client->GetToken()] = client;
    client->Init();

    std::weak_ptr wclient{client};
    std::weak_ptr<Server> wself{shared_from_this()};

    // io_context.stop()이 Server 소멸보다 먼저 호출될 것을 보장할 것
    client->SetPacketHandler(
        LoginMessage_Type::REQUEST_LOGIN,
        [wself, wclient](char *serializedData, int length)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    s->HandleRequestLogin(c, serializedData, length);
                }
            }
        });

    client->SetDisconnectHandler(
        EM_Type::EM_DISCONNECTED,
        [wself, wclient](system::error_code &ec)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    c->Stop();
                    s->RemoveClient(c->GetToken());
                }
            }
        });
}

void Server::HandleRequestLogin(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    M_RequestLogin msg;
    std::cout << std::format("c use_count: {}\n", client.use_count());
    M_ResponseLogin rl;

    if (msg.ParseFromArray(serializedData, length))
    {
        std::string nickname{msg.nickname()};

        client->SetNickname(nickname);

        {
            std::string reason;
            bool res = _lm->RequestEnterLobby(client, reason);

            if (res)
            {
                rl.set_result(client->GetToken());
                rl.set_reason("ok");

                RemoveClient(client->GetToken());
            }
            else
            {
                rl.set_result(0);
                rl.set_reason(std::move(reason));
            }
        }
    }
    else
    {
        rl.set_result(0);
        rl.set_reason("Parsing error");
    }

    std::string rlString{rl.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LoginMessage_Type::RESPONSE_LOGIN),
        static_cast<int>(rlString.length()) + 12);
    t.insert(t.end(), rlString.begin(), rlString.end());

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

    _lm->PrintStatus();
}