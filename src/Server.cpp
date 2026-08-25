#include "Server.h"
#include <iostream>
#include <chrono>
#include "authentication_message.pb.h"
#include "error_message.pb.h"
#include "redisInterface.h"
#include "util.h"
#include "sha256.h"

using namespace boost;

Server::Server(
    asio::io_context &io,
    std::shared_ptr<IRedis> rs)
    :
    _io(io),
    _rs(rs),
    _timer(io)
{
    //_udpRecvBuffer = std::shared_ptr<char[]>(
    // new (std::nothrow) char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    // ASSERT(_udpRecvBuffer != nullptr, "Server. Failed to allocate UDP receive buffer");
}

void Server::RemoveClient(uint64_t token)
{
    std::scoped_lock sl{_connMtx};
    _rs->Del(std::format("client:{}", token));
    _connectedClients.erase(token);
}

void Server::Stop()
{
    std::scoped_lock sl{_connMtx};

    for (auto &c : _connectedClients)
    {
        c.second->Stop();
    }

    _connectedClients.clear();
}

int Server::AddClient(std::shared_ptr<IClient> client)
{
    std::scoped_lock sl{_connMtx};
    _connectedClients[client->GetToken()] = client;
    client->Init();

    std::weak_ptr wclient{client};
    std::weak_ptr<Server> wself{shared_from_this()};

    auto now = std::chrono::system_clock().now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    _rs->HashSet(
        std::format("client:{}", client->GetToken()),
        "ConnectedTime",
        std::to_string(ms.count()));

    // io_context.stop()이 Server 소멸보다 먼저 호출될 것을 보장할 것
    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_LOGIN,
        [wself, wclient](char *serializedData, int length)
        {
            auto s = wself.lock();
            auto c = wclient.lock();

            if (s != nullptr && c != nullptr)
            {
                s->HandleRequestLogin(c, serializedData, length);
            }
        });

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_REGISTER_ACCOUNT,
        [wself, wclient](char *serializedData, int length)
        {
            auto s = wself.lock();
            auto c = wclient.lock();

            if (s != nullptr && c != nullptr)
            {
                s->HandleRequestCreationAccount(c, serializedData, length);
            }
        });

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_PLAYER_DATA,
        [wself, wclient](char *serializedData, int length)
        {
            auto s = wself.lock();
            auto c = wclient.lock();

            if (s != nullptr && c != nullptr)
            {
                s->HandleRequestPlayerData(c, serializedData, length);
            }
        });

    client->SetPacketHandler(
        ProtoAuthenticationMessage::REQUEST_LOGOUT,
        [wself, wclient](char *serializedData, int length)
        {
            auto s = wself.lock();
            auto c = wclient.lock();

            if (s != nullptr && c != nullptr)
            {
                s->HandleRequestLogout(c, serializedData, length);
            }
        });

    client->SetErrorHandler(
        EM_Type::EM_DISCONNECTED,
        [wself, wclient](system::error_code &ec)
        {
              auto s = wself.lock();
            auto c = wclient.lock();

            if (s != nullptr && c != nullptr)
            {
                auto token = c->GetToken();
                s->LogoutClient(c);
                c->Stop();
                s->RemoveClient(token);
            }
        });

    return 0;
}

void Server::HandleRequestCreationAccount(std::shared_ptr<IClient> client, char *serializedData, int length)
{
    PMRequestRegisterAccount receivedMessage;
    PMResponseRegisterAccount responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto id = receivedMessage.id();
        auto password = receivedMessage.password();
        auto nickname = receivedMessage.nickname();
        auto personalColor = receivedMessage.personalcolor();

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
        else if (_rs->Exists(idStr))
        {
            valid = false;
            responseMessage.set_message("idAleadyInUse");
        }

        if (valid)
        {
            std::string passwordHashStr{sha256(password)};

            std::cout << std::format("A new account has been created {} {} {}\n", 
                id, password, personalColor);
            _rs->HashSet(idStr, "password", passwordHashStr);
            _rs->HashSet(idStr, "nickname", nickname);
            _rs->HashSet(idStr, "personalColor", personalColor);
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

void Server::HandleRequestLogin(std::shared_ptr<IClient> client, char *serializedData, int length)
{
    PMRequestLogin receivedMessage;
    PMResponseLogin responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto idStr{receivedMessage.id()};
        auto idKeyStr{std::format("id:{}", receivedMessage.id())};
        auto passwordHashStr{sha256(receivedMessage.password())};
        auto loginedKeyStr{std::format("logined:{}", idStr)};
        bool valid = true;
        // bool res = _lm->RequestEnterLobby(client, reason);

        if (_rs->Exists(idKeyStr) == false)
        {
            valid = false;
            responseMessage.set_message("idDoesNotExsist");
        }
        else if ((*(_rs->HashGet(idKeyStr, "password"))).compare(passwordHashStr) != 0)
        {
            valid = false;
            responseMessage.set_message("passwordMismatch");
        }
        else if (_rs->Exists(loginedKeyStr))
        {
            valid = false;
            responseMessage.set_message("loginedAlready");
        }

        if (valid)
        {
            auto now = std::chrono::system_clock().now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            auto clientKeyStr{std::format("client:{}", client->GetToken())};

            _rs->HashSet(clientKeyStr, "loginId", idStr);
            _rs->HashSet(loginedKeyStr, "token", std::to_string(client->GetToken()));
            _rs->HashSet(loginedKeyStr, "loginTime", std::to_string(ms.count()));
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
    std::shared_ptr<IClient> client,
    char *serializedData, int length)
{
    PMRequestPlayerData receivedMessage;
    PMResponsePlayerData responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto clientTokenStr = std::format("client:{}", client->GetToken());
        std::string idStr;
        std::string idKeyStr;
        bool valid = true;

        if (!_rs->Exists(clientTokenStr))
        {
            valid = false;
            responseMessage.set_message("invalidToken");
        }
        
        if (valid) 
        {
            auto loginId = _rs->HashGet(clientTokenStr, "loginId");
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
            if (!_rs->Exists(idKeyStr))
            {
                valid = false;
                responseMessage.set_message("invalidId");
            }
        }

        if (valid)
        {
            auto nicknameStr = _rs->HashGet(idKeyStr, "nickname");
            auto personalColorStr = _rs->HashGet(idKeyStr, "personalColor");
            _rs->Persist(idKeyStr);

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
    std::shared_ptr<IClient> client,
    char *serializedData, int length)
{
    PMRequestLogout receivedMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        LogoutClient(client);
    }
}

void Server::LogoutClient(std::shared_ptr<IClient> client)
{
    auto token = client->GetToken();
    auto clientKey{std::format("client:{}", token)};

    if (!_rs->Exists(clientKey))
    {
        std::cout << std::format("Disconnected from client({})", client->GetToken());
        return;
    }

    auto loginIdStr = _rs->HashGet(clientKey, "loginId");
    if (!loginIdStr)
    {
        std::cout << std::format("Disconnected from client(not logined, {})\n", client->GetToken());
        return;
    }

    auto loginIdKeyStr{std::format("logined:{}", *loginIdStr)};
    std::string noLoginedStr{"noLogined"};
    client->SetLoginKey(noLoginedStr);

    _rs->Del(loginIdKeyStr);
    _rs->HashDel(clientKey, "loginId");
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