#include "Server.h"
#include <iostream>
#include <chrono>
// #include "isolation_pb/login_message.pb.h"
#include "isolation_pb/authentication_message.pb.h"
#include "isolation_pb/error_message.pb.h"
#include "redisService.h"
#include "util.h"
#include "sha256.h"

using namespace boost;

Server::Server(
    asio::io_context &io)
    : _io(io),
      _udpSocket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 51022)),
      _timer(io)
{
    //_udpRecvBuffer = std::shared_ptr<char[]>(
    // new (std::nothrow) char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    // ASSERT(_udpRecvBuffer != nullptr, "Server. Failed to allocate UDP receive buffer");
}

// void Server::CacheLobbyList()
// {
//     std::scoped_lock sl{_lobbyCacheMtx};

//     // lobby cache를 갱신

//     _timer.expires_after(std::chrono::milliseconds(static_cast<int>(1000)));
//     _timer.async_wait(
//         [ws = std::weak_ptr{shared_from_this()}](const boost::system::error_code &ec)
//         {
//             std::cout << "cache!";
//             if (!ec)
//             {
//                 if (auto s = ws.lock())
//                 {
//                     s->CacheLobbyList();
//                 }
//             }
//             else
//             {
//                 std::cerr << "timer error: " << ec.what() << std::endl;
//             }
//         });
// }

void Server::RemoveClient(uint64_t token)
{
    std::scoped_lock sl{_connMtx};
    auto& rs = RS::Instance();
    rs.Del(std::format("client:{}", token));
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

    auto &rs = RS::Instance();
    auto now = std::chrono::system_clock().now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    rs.HashSet(
        std::format("client:{}", client->GetToken()),
        "ConnectedTime",
        std::to_string(ms.count()));

    // io_context.stop()이 Server 소멸보다 먼저 호출될 것을 보장할 것
    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_LOGIN,
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

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_REGISTER_ACCOUNT,
        [wself, wclient](char *serializedData, int length)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    s->HandleRequestCreationAccount(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_PLAYER_DATA,
        [wself, wclient](char *serializedData, int length)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    s->HandleRequestPlayerData(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_LOGOUT,
        [wself, wclient](char *serializedData, int length)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    s->HandleRequestLogout(c, serializedData, length);
                }
            }
        });

    client->SetErrorHandler(
        EM_Type::EM_DISCONNECTED,
        [wself, wclient](system::error_code &ec)
        {
            if (auto s = wself.lock())
            {
                if (auto c = wclient.lock())
                {
                    auto token = c->GetToken();
                    s->LogoutClient(c);
                    c->Stop();
                    s->RemoveClient(token);
                }
            }
        });
}

void Server::HandleRequestCreationAccount(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    PMRequestRegisterAccount receivedMessage;
    PMResponseRegisterAccount responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto id = receivedMessage.id();
        auto password = receivedMessage.password();
        auto nickname = receivedMessage.nickname();
        auto personalColor = receivedMessage.personalcolor();
        
        // std::cout << std::format(
        //     "Create Account ---\n{}------------------\n",
        //     receivedMessage.DebugString());
        
        auto &rs = RS::Instance();
        std::string idStr{std::format("id:{}", id)};
        bool valid = true;

        if (id.length() < 2)
        {
            valid = false;
            responseMessage.set_message("idLength<2");
        }
        else if (password.length() < 2)
        {
            valid = false;
            responseMessage.set_message("passwordLength<2");
        }
        else if (nickname.length() < 2)
        {
            valid = false;
            responseMessage.set_message("nicknameLength<2");
        }
        else if (rs.Exists(idStr))
        {
            valid = false;
            responseMessage.set_message("idAleadyInUse");
        }

        if (valid)
        {
            std::string passwordHashStr{sha256(password)};

            std::cout << std::format("A new account has been created {} {} {}\n", 
                id, password, personalColor);
            rs.HashSet(idStr, "password", passwordHashStr);
            rs.HashSet(idStr, "nickname", nickname);
            rs.HashSet(idStr, "personalColor", personalColor);
            // rs.Expire(idStr, 60);

            responseMessage.set_message("ok");
        }

        responseMessage.set_result(valid);
    }
    else
    {
        responseMessage.set_result(false);
        responseMessage.set_message("parsingError");
    }

    // Sleep(3000);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string responseStr{responseMessage.SerializeAsString()};
    std::vector<char> t;
    append_prot_packet(
        t,
        static_cast<int>(ProtoAuthenticationMessage::RESPONSE_REGISTER_ACCOUNT),
        static_cast<int>(responseStr.length() + 12));
    t.insert(t.end(), responseStr.begin(), responseStr.end());

    client->PostWrite(t);
}

void Server::HandleRequestLogin(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    PMRequestLogin receivedMessage;
    PMResponseLogin responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto &rs = RS::Instance();
        auto idStr{receivedMessage.id()};
        auto idKeyStr{std::format("id:{}", receivedMessage.id())};
        auto passwordHashStr{sha256(receivedMessage.password())};
        auto loginedKeyStr{std::format("logined:{}", idStr)};
        bool valid = true;
        // bool res = _lm->RequestEnterLobby(client, reason);

        if (rs.Exists(idKeyStr) == false)
        {
            valid = false;
            responseMessage.set_message("idDoesNotExsist");
        }
        else if ((*rs.HashGet(idKeyStr, "password")).compare(passwordHashStr) != 0)
        {
            valid = false;
            responseMessage.set_message("passwordMismatch");
        }
        else if (rs.Exists(loginedKeyStr))
        {
            valid = false;
            responseMessage.set_message("loginedAlready");
        }

        if (valid)
        {
            auto now = std::chrono::system_clock().now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            auto clientKeyStr{std::format("client:{}", client->GetToken())};

            rs.HashSet(clientKeyStr, "loginId", idStr);
            rs.HashSet(loginedKeyStr, "token", std::to_string(client->GetToken()));
            rs.HashSet(loginedKeyStr, "loginTime", std::to_string(ms.count()));
            client->SetLoginKey(loginedKeyStr);

            responseMessage.set_result(true);
            responseMessage.set_message("ok");
        }
    }
    else
    {
        responseMessage.set_result(false);
        responseMessage.set_message("parsingError");
    }

    std::string responseStr{responseMessage.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(ProtoAuthenticationMessage::RESPONSE_LOGIN),
        static_cast<int>(responseStr.length()) + 12);
    t.insert(t.end(), responseStr.begin(), responseStr.end());

    client->PostWrite(t);
}

void Server::HandleRequestPlayerData(
    std::shared_ptr<ClientSocket> client,
    char *serializedData, int length)
{
    PMRequestPlayerData receivedMessage;
    PMResponsePlayerData responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto &rs = RS::Instance();
        auto clientTokenStr = std::format("client:{}", client->GetToken());
        std::string idStr;
        std::string idKeyStr;
        bool valid = true;

        if (!rs.Exists(clientTokenStr))
        {
            valid = false;
            responseMessage.set_message("invalidToken");
        }
        
        if (valid) 
        {
            auto loginId = rs.HashGet(clientTokenStr, "loginId");
            if (!loginId)
            {
                valid = false;
                responseMessage.set_message("needLogin");
            }
            else
            {
                idStr = *loginId;
            }
        }

        if (valid)
        {
            idKeyStr = std::format("id:{}", idStr);
            if (!rs.Exists(idKeyStr))
            {
                valid = false;
                responseMessage.set_message("invalidId");
            }
        }

        if (valid)
        {
            auto nicknameStr = rs.HashGet(idKeyStr, "nickname");
            auto personalColorStr = rs.HashGet(idKeyStr, "personalColor");
            rs.Persist(idKeyStr);

            responseMessage.set_result(true);
            responseMessage.set_message("ok");
            responseMessage.set_nickname(*nicknameStr);
            responseMessage.set_personalcolor(*personalColorStr);
        }
    }
    else
    {
        responseMessage.set_result(false);
        responseMessage.set_message("parsingError");
    }

    std::string responseStr{responseMessage.SerializeAsString()};
    std::vector<char> t;
    append_prot_packet(
        t,
        static_cast<int>(ProtoAuthenticationMessage::RESPONSE_PLAYER_DATA),
        static_cast<int>(responseStr.length() + 12));
    t.insert(t.end(), responseStr.begin(), responseStr.end());

    client->PostWrite(t);
}

void Server::HandleRequestLogout(
    std::shared_ptr<ClientSocket> client,
    char *serializedData, int length)
{
    PMRequestLogout receivedMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        LogoutClient(client);
    }
}

void Server::LogoutClient(std::shared_ptr<ClientSocket> client)
{
    auto token = client->GetToken();
    auto &rs = RS::Instance();

    auto clientKey{std::format("client:{}", token)};

    if (!rs.Exists(clientKey))
    {
        std::cout << std::format("Disconnected from client({})", client->GetToken());
        return;
    }

    auto loginIdStr = rs.HashGet(clientKey, "loginId");
    if (!loginIdStr)
    {
        std::cout << std::format("Disconnected from client(not logined, {})\n", client->GetToken());
        return;
    }

    auto loginIdKeyStr{std::format("logined:{}", *loginIdStr)};
    std::string noLoginedStr{"noLogined"};
    client->SetLoginKey(noLoginedStr);

    rs.Del(loginIdKeyStr);
    rs.HashDel(clientKey, "loginId");
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

    // _lm->PrintStatus();
}

void Server::StartUDPReceive()
{
    for (int i = 0; i < 8; ++i)
    {
        std::cout << std::format("Receive UDP data {}", i);
        ReceiveUDP();
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