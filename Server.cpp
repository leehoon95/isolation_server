#include "Server.h"
#include <iostream>
#include "protoc/proto_message.pb.h"
#include "util.h"

using namespace boost;

Server::Server(
    asio::io_context &io)
    : _io(io),
      _udpSocket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 51022)),
      _room(io, _udpSocket)
{
    _udpRecvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    _udpSendBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::SEND_BUFFER_SIZE))]);

    StartUDPReceive();
}

void Server::StartUDPReceive()
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

                //std::cout << std::endl;

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

                        if (rcp.ParseFromArray(&_udpRecvBuffer[12], length - 12)) {
                            std::cout << std::format("UDP data received. ri: {}, ci: {}\n", rcp.roomindex(), rcp.transform().clientindex());
                           _room.ReportClientTransform(rcp.mutable_transform(), _remoteEndpoint);
                        }
                        else {
                            std::cerr << "UDP ObjectTransform parsing error.\n";
                        }
                    }
                }

                StartUDPReceive();
            }
            else
            {
                print_boost_system_error("UDP async_receive_from.error.", ec);
                std::cout << std::format("UDP error: {}\n", ec.what());
            }
        });
}

// void SendUDPBuffer()
// {

// }

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
    client->SetProcedure(
        REQUEST_LOGIN,
        [this, client](char *serializedData, int length)
        {
            PROTO_RequestLogin msg;
            //std::cout << std::format("c use_count: {}\n", client.use_count());

            if (msg.ParseFromArray(serializedData, length))
            {
                std::string nickname{msg.nickname()};

                client->SetNickname(nickname);

                PROTO_LoginResult lr;
                
                {
                    std::scoped_lock sl{_connMtx, _loginedMtx};
                    int clientIndex = client->GetIndex();
                    auto cc = _connectedClients.find(clientIndex);
                    if (cc == _connectedClients.end())
                    {
                        std::cout << std::format("Not found connected client {}:{}\n",
                                                 clientIndex, nickname);
                        // do nothing on the server side.
                        return;
                    }
                    if (_loginedClients.find(clientIndex) != _loginedClients.end())
                    {
                        lr.set_clientindex(-1);
                        // lr.set_roomindex(-1);
                        lr.set_reason("Not found as connected client");
                    }
                    else
                    {
                        lr.set_clientindex(clientIndex);
                        // lr.set_roomindex(100);
                        _loginedClients[clientIndex] = cc->second;
                        _connectedClients.erase(cc);
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
                client->PostWrite(t);
                std::cout << std::format("t.size(): {}\n", t.size());
            }
            else
            {
                std::string message{"invalid nickname"};
                std::vector<char> t(message.begin(), message.end());
                client->PostWrite(t);
                // std::cout << std::format("t.size(): {}\n", t.size());
            }
        });

    client->SetProcedure(
        REQUEST_SYNC,
        [this, client](char *serializedData, int length)
        {
            _room.EnterRoom(client);

            PROTO_RequestSyncResult rsr;
            rsr.set_roomindex(100);

            std::string rsrString{rsr.SerializeAsString()};

            std::vector<char> t;
            append_prot_packet(
                t, 
                static_cast<int>(REQUEST_SYNC_RESULT), 
                static_cast<int>(rsrString.length()) + 12);
            t.insert(t.end(), rsrString.begin(), rsrString.end());

            client->PostWrite(t);
        });

    client->OnDisconnected(
        [this](int index, std::string nickname)
        {
            std::cout << std::format("client {} / {} disconnected\n", index, nickname);
            {
                std::scoped_lock sl{_connMtx, _loginedMtx};
                _connectedClients.erase(index);
                _room.ExitRoom(index);
                _loginedClients.erase(index);
            }
        });
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

    _room.PrintStatus();
}