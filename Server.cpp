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
      _udpSocket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 51022))
{
    //_udpRecvBuffer = std::shared_ptr<char[]>(
        //new (std::nothrow) char[static_cast<size_t>(static_cast<size_t>(UDPBufferSize::RECV_BUFFER_SIZE))]);

    //ASSERT(_udpRecvBuffer != nullptr, "Server. Failed to allocate UDP receive buffer");

    // try
    // {
    //     _lm = std::make_shared<LobbyManager>(io, _udpSocket);
    //     _lm->StartRefreshSessionCache();
    // }
    // catch (const std::bad_alloc &e)
    // {
    //     ASSERT(false, "Server. Failed to allocate LobbyManager");
    // }
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
                    s->HandlerRequestPlayerData(c, serializedData, length);
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
                    c->Stop();
                    s->RemoveClient(c->GetToken());
                }
            }
        });
}

void Server::HandleRequestLogin(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    PMRequestLogin receivedMessage;
    std::cout << std::format("c use_count: {}\n", client.use_count());
    PMResponseLogin responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto &rs = RS::Instance();    
        std::string idStr{std::format("id:{}", receivedMessage.id())};
        std::string passwordHashStr{sha256(receivedMessage.password())};
        bool valid = true;
        // bool res = _lm->RequestEnterLobby(client, reason);

        if (rs.Exists(idStr) == false)
        {
            valid = false;
            responseMessage.set_message("idDoesNotExsist");
        }
        else if ((*rs.HashGet(idStr, "password")).compare(passwordHashStr) != 0)
        {
            valid = false;
            responseMessage.set_message("passwordMismatch");
        }

        if (valid)
        {
            auto token = client->GetToken();
            auto authTokenStr{std::format("authorized:{}", token)};
            auto now = std::chrono::system_clock().now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            
            rs.HashSet(authTokenStr, "id", receivedMessage.id());
            rs.HashSet(authTokenStr, "loginTime", std::to_string(ms.count()));\
            
            responseMessage.set_result(true);
            responseMessage.set_message("ok");
            responseMessage.set_token(token);
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
     
void Server::HandleRequestCreationAccount(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    PMRequestRegisterAccount receivedMessage;
    PMResponseRegisterAccount responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto id = receivedMessage.id();
        auto password = receivedMessage.password();
        auto nickname = receivedMessage.nickname(); 
        uint32_t pch = receivedMessage.pch();
        uint32_t pcs = receivedMessage.pcs();
        uint32_t pcv = receivedMessage.pcv();

        auto &rs = RS::Instance();
        std::string idStr{std::format("id:{}", id)};
        bool valid = true;

        if (id.length() < 2) 
        {
            valid = false;
            responseMessage.set_message("idLengthIs<2");
        }
        else if (password.length() < 2) 
        {
            valid = false;
            responseMessage.set_message("passwordLengthIs<2");
        }
        else if (rs.Exists(idStr)) 
        {
            valid = false;
            responseMessage.set_message("idIsAleadyInUse");
        }

        if (valid)
        {
            std::string passwordHashStr{sha256(password)};
            std::string pcStr{std::format("{}/{}/{}", pch, pcs, pcv)};

            std::cout << std::format("New account {} {} {}\n", id, password, pcStr);
            rs.HashSet(idStr, "password", passwordHashStr);
            rs.HashSet(idStr, "nickname", nickname);
            rs.HashSet(idStr, "personalColor", pcStr);
            rs.Expire(idStr, 60);

            responseMessage.set_message("ok");
        }
        
        responseMessage.set_result(valid);
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
        static_cast<int>(ProtoAuthenticationMessage::RESPONSE_REGISTER_ACCOUNT),
        static_cast<int>(responseStr.length() + 12));
    t.insert(t.end(), responseStr.begin(), responseStr.end());

    client->PostWrite(t);
}

void Server::HandlerRequestPlayerData(
    std::shared_ptr<ClientSocket> client, 
    char *serializedData, int length)
{
    PMRequestPlayerData receivedMessage;
    PMResponsePlayerData responseMessage;

    if (receivedMessage.ParseFromArray(serializedData, length))
    {
        auto &rs = RS::Instance(); 
        auto authTokenStr = std::format("authorized:{}", receivedMessage.token());

        if (rs.Exists(authTokenStr))
        {
            auto idKeyStr = std::format("id:{}", *rs.HashGet(authTokenStr, "id"));
            auto nicknameStr = rs.HashGet(idKeyStr, "nickname");
            auto personalColorStr = rs.HashGet(idKeyStr, "personalColor");

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