#include "Server.h"
#include <iostream>
#include "protoc/proto_message.pb.h"
#include "util.h"

using namespace boost;

Server::Server(
    asio::io_context &io)
    : _io(io),
      _udpSocket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 51022)),
      //_room(io, _udpSocket),
      _rm(io, _udpSocket)
{
    _udpRecvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    // _udpSendBuffer = std::shared_ptr<char[]>(
    //     new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::SEND_BUFFER_SIZE))]);
}

void Server::ReceiveUDP()
{
    // if (!_udpRecvBuffer)
    // {
    //     std::cout << "UDP Recv buffer is not initialized.(maybe alloc exception.)\n";
    //     return;
    // }

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
                    else if (type == REPORT_CHARACTER_PHYSICS)
                    {
                        PROTO_ReportCharacterPhysics rcp;

                        if (rcp.ParseFromArray(&_udpRecvBuffer[12], length - 12))
                        {
                            std::cout << std::format("UDP data received. ri: {}, ci: {}\n", rcp.roomindex(), rcp.transform().clientindex());
                            //_room.ReportClientTransform(rcp.mutable_transform(), _remoteEndpoint);
                        }
                        else
                        {
                            std::cerr << "UDP ObjectTransform parsing error.\n";
                        }
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

bool Server::MoveClientToLogin(std::shared_ptr<ClientSocket> client, std::string nickname)
{
    std::scoped_lock sl{_connMtx, _loginedMtx};

    int clientIndex = client->GetIndex();
    auto cc = _connectedClients.find(clientIndex);

    if (cc == _connectedClients.end())
    {
        std::cout << std::format("Not found connected client {}:{}\n",
                                 clientIndex, nickname);
        // do nothing on the server side.
        return false;
    }

    if (_loginedClients.find(clientIndex) != _loginedClients.end())
    {
        std::cout << std::format("client {}:{} has aleady logined\n",
                                 clientIndex, nickname);

        return false;
    }
    else
    {
        _loginedClients[clientIndex] = cc->second;
        _connectedClients.erase(cc);

        return true;
    }
}

void Server::RemoveClient(int index)
{
    std::scoped_lock sl{_connMtx, _loginedMtx};
    _connectedClients.erase(index);
    _loginedClients.erase(index);
}

void Server::PassClientToRoomManager(std::shared_ptr<ClientSocket> client)
{
    _rm.LoginedClient(client);
}

void Server::Stop()
{
    std::scoped_lock sl{_connMtx, _loginedMtx};

    for (auto &c : _connectedClients)
    {
        c.second->Stop();
    }

    for (auto &c : _loginedClients)
    {
        c.second->Stop();
    }

    _connectedClients.clear();
    _loginedClients.clear();
}

void Server::AddClient(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_connMtx};
    _clientIndex++;
    _connectedClients[_clientIndex] = client;
    client->Init(_clientIndex);

    std::weak_ptr wclient{client};
    auto self = shared_from_this();

    client->SetPacketHandler(
        PROTO_MessageType::REQUEST_LOGIN,
        [self, wclient](char *serializedData, int length)
        {
            if (auto c = wclient.lock())
            {
                PROTO_RequestLogin msg;
                // std::cout << std::format("c use_count: {}\n", client.use_count());

                if (msg.ParseFromArray(serializedData, length))
                {
                    std::string nickname{msg.nickname()};

                    c->SetNickname(nickname);

                    PROTO_LoginResult lr;

                    {
                        bool res = self->MoveClientToLogin(c, nickname);

                        if (res)
                        {

                            lr.set_clientindex(c->GetIndex());
                        }
                        else
                        {
                            lr.set_clientindex(-1);
                            lr.set_reason("Not found as connected client");
                        }
                    }

                    std::string message{std::format("client nickname: {}. login allowed.", nickname)};
                    std::cout << message << std::endl;
                    // for (auto &c : message)
                    // {
                    //     printf("0x%X(%c) ", c, c);
                    // }

                    // printf("\n");

                    std::string lrString{lr.SerializeAsString()};

                    std::vector<char> t;
                    append_prot_packet(
                        t,
                        static_cast<int>(LOGIN_RESULT),
                        static_cast<int>(lrString.length()) + 12);
                    t.insert(t.end(), lrString.begin(), lrString.end());
                    // t.push_back(message.begin(), message.end());
                    c->PostWrite(t);
                    //std::cout << std::format("t.size(): {}\n", t.size());
                }
                else
                {
                    std::string message{"invalid nickname"};
                    std::vector<char> t(message.begin(), message.end());
                    c->PostWrite(t);
                    // std::cout << std::format("t.size(): {}\n", t.size());
                }
            }
        });

    client->SetPacketHandler(
        PROTO_MessageType::REQUEST_SYNC,
        [self, wclient](char *serializedData, int length)
        {
            if (auto c = wclient.lock())
            {
                c->ClearHandler();

               // self->PassClientToRoomManager(c);

                PROTO_RequestSyncResult rsr;
                rsr.set_roomindex(100);

                std::string rsrString{rsr.SerializeAsString()};

                std::vector<char> t;
                append_prot_packet(
                    t,
                    static_cast<int>(REQUEST_SYNC_RESULT),
                    static_cast<int>(rsrString.length()) + 12);
                t.insert(t.end(), rsrString.begin(), rsrString.end());

                c->PostWrite(t);
            }
        });

    client->SetPacketHandler(
        PROTO_MessageType::DISCONNECTED,
        [self, wclient](char *serializedData, int length)
        {
            if (auto c = wclient.lock())
            {
                int index = c->GetIndex();

                std::cout << std::format("client {} / {} disconnected\n", index, c->GetNickname());

                self->RemoveClient(index);
            }
        });
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
    std::scoped_lock sl{_connMtx, _loginedMtx};

    std::cout << "--- just connected clients ---\n";
    for (auto &pair : _connectedClients)
    {
        std::cout << std::format("{}: ---\n", pair.first);
    }
    std::cout << "------------------------------\n\n";

    std::cout << "--- logined client ---\n";
    for (auto &pair : _loginedClients)
    {
        if (pair.second.use_count() > 0)
            std::cout << std::format("{}: {}\n", pair.first, pair.second->GetNickname());
    }
    std::cout << "-----------------------\n\n";

    _rm.PrintStatus();
}