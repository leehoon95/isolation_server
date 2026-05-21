#include "lobbyManager.h"
#include "util.h"
#include "isolation_pb/lobby_message.pb.h"
#include "redisService.h"

using namespace boost;

LobbyManager::LobbyManager(asio::io_context &io, asio::ip::udp::socket &socket)
    : _io(io),
      _udpSocket(socket),
      _timer(io)
{
}

bool LobbyManager::RequestEnterLobby(std::shared_ptr<ClientSocket> client, std::string &reason)
{
    if (!client)
    {
        reason = "LobbyManager::RequestEnterLobby: Client is not valid.";
        return false;
    }

    std::string nickname = client->GetNickname();

    try
    {
        if (utf8_length(nickname) < 2)
        {
            reason = "LobbyManager::RequestEnterLobby: Client nickname is not valid.";
            return false;
        }
    }
    catch (std::runtime_error &e)
    {
        reason = "Invalid UTF-8 string";
        return false;
    }

    std::scoped_lock<std::mutex> sl{_loginedClientMtx};

    RS &rs = RS::Instance();

    auto keyUser = std::format("client:{}", client->GetToken());
    bool keyExists = rs.Exists(keyUser); // nickname 중복검사

    if (keyExists)
    {
        reason = "This nickname is already exists";
        return false;
    }

    _loginedClients[client->GetToken()] = client;
    rs.Set(keyUser, nickname);
    // rs.Expire(keyUser, 5);

    SetClientHandler(client);

    reason = "ok";

    return true;
}

void LobbyManager::DisconnectedClient(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_loginedClientMtx};

    client->Stop();
    auto token = client->GetToken();

    _loginedClients.erase(token);

    RS &rs = RS::Instance();
    auto keyUser = std::format("client:{}", token);
    rs.Del(keyUser);
}

void LobbyManager::SetClientHandler(std::shared_ptr<ClientSocket> client)
{
    client->ClearPacketHandler();
    client->ClearDisconnectHandler();

    std::weak_ptr<ClientSocket> wc{client};
    std::weak_ptr<LobbyManager> wlm{shared_from_this()};

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_SESSION_LIST,
        [wlm, wc](char *serializedData, int length)
        {
            auto c = wc.lock();
            auto lm = wlm.lock();

            if (c && lm)
            {
                lm->SendCurrentActivedSessionList(c);
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_SESSION_CREATION,
        [wlm, wc](char *serializedData, int length)
        {
            auto c = wc.lock();
            auto lm = wlm.lock();

            if (c && lm)
            {
                lm->HandleRequestSessionCreate(c, serializedData, length);
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_SESSION_ENTRY,
        [wlm, wc](char *serializedData, int length)
        {
            auto c = wc.lock();
            auto lm = wlm.lock();

            if (c && lm)
            {
                lm->HandleRequestSessionEnter(c, serializedData, length);
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_LOBBY_EXIT,
        [wlm, wc](char *serializedData, int length)
        {
            auto c = wc.lock();
            auto lm = wlm.lock();

            if (c && lm)
            {
                lm->HandleRequestSessionLeave(c, serializedData, length);
            }
        });

    client->SetDisconnectHandler(
        LobbyMessage_Type::LBMT_DISCONNECTED,
        [wlm, wc](system::error_code &ec)
        {
            auto c = wc.lock();
            auto lm = wlm.lock();

            if (c && lm)
            {
                lm->DisconnectedClient(c);
            }
        });
}

void LobbyManager::RewriteSessionListCache()
{
    {
        std::scoped_lock sl{_sessionsMtx, _RSLCacheMtx};

        // auto ml = _RSLCache.mutable_list();
        // ml->Clear();

        M_ResponseSessionList rsl;

        rsl.set_count(_sessions.size());
        auto ml = rsl.mutable_list();

        if (_sessions.size() > 0)
        {
            int sessionIndex;
            std::string sessionName;
            int maxClientCount;
            int clientCount;
            std::string password;
            std::string joinCode;

            for (auto &var : _sessions)
            {
                bool res = var.second->GetSessionInfo(
                    sessionName,
                    maxClientCount,
                    clientCount,
                    password,
                    joinCode);

                if (!res)
                    continue;

                auto si = ml->Add();
                si->set_sessionindex(var.first);
                si->set_sessionname(sessionName);
                si->set_maxclientcount(maxClientCount);
                si->set_clientcount(clientCount);
                si->set_password(password);
                si->set_joincode(joinCode);
            }

            _serializedRSLCache = rsl.SerializeAsString();
        }
        else
        {
            M_ResponseSessionList rsl;

            rsl.set_count(0);

            _serializedRSLCache = rsl.SerializeAsString();
        }

        std::cout << std::format("sessions : {}\n", _sessions.size());
    }

    std::weak_ptr<LobbyManager> wlm{shared_from_this()};
    _timer.expires_after(std::chrono::milliseconds(1000));
    _timer.async_wait([wlm](const system::error_code &ec)
                      {
        if (!ec) {
            if (auto lm = wlm.lock())
            {
                lm->RewriteSessionListCache();
            }
        }
        else {
            std::cerr << std::format("LobbyManager::RefreshActivedSessionCache _timer.async_wait error. what: {}\n", ec.message());
        } });
}

// std::weak_ptr<Room> LobbyManager::GetRoom(int roomIndex)
// {
//     std::scoped_lock<std::mutex> sl{_roomsMtx};
//     if (_rooms.find(roomIndex) == _rooms.end())
//         return std::weak_ptr<Room>();

//     return _rooms[roomIndex];
// }

void LobbyManager::PrintStatus()
{
}

void LobbyManager::SendCurrentActivedSessionList(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_RSLCacheMtx};

    //std::string serial{_serializedRSLCache};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_SESSION_LIST),
        static_cast<int>(_serializedRSLCache.length()) + 12);
    t.insert(t.end(), _serializedRSLCache.begin(), _serializedRSLCache.end());

    client->PostWrite(t);
}

int LobbyManager::CreateSession(
    std::shared_ptr<ClientSocket> client,
    const std::string_view name,
    const std::string_view password,
    std::string &reason)
{
    auto session = std::make_shared<Session>();

    bool res = session->CreateSession(
        client,
        4,
        name,
        password,
        reason);

    if (!res)
        return -1;

    int sessionIndex = -1;
    {
        std::scoped_lock<std::mutex> sl{_emptySessionsMtx};
        sessionIndex = ++_sessionIndexCounter;

        _emptySessions[sessionIndex] = session;

        session->SetReceiveJoinCodeCallback(
            [wlobby = std::weak_ptr<LobbyManager>(shared_from_this()), sessionIndex]()
            {
                if (auto l = wlobby.lock())
                {
                    l->ActivateSession(sessionIndex);
                }
            });
    }

    reason = "ok";

    return sessionIndex;
}

void LobbyManager::ActivateSession(int sessionIndex)
{
    std::scoped_lock sl(_emptySessionsMtx, _sessionsMtx);

    if (_emptySessions.find(sessionIndex) == _emptySessions.end())
    {
        std::cerr << std::format("LobbyManager::ActivateSession. Invalid session index {}\n", sessionIndex);
        return;
    }
    auto ss = _emptySessions[sessionIndex];
    _sessions[sessionIndex] = ss;
    _emptySessions.erase(sessionIndex);

    std::cout << std::format("Activate session:{}\n", ss->GetSessionToken());
}

void LobbyManager::HandleRequestSessionCreate(
    std::shared_ptr<ClientSocket> client,
    char *serializedData,
    int length)
{
    M_RequestSessionCreation rcs;
    M_ResponseSessionCreation response;

    if (rcs.ParseFromArray(serializedData, length))
    {
        std::string name{rcs.sessionname()};
        std::string password{rcs.password()};

        std::string reason;
        int res = CreateSession(client, name, password, reason);

        if (res >= 0)
        {
            std::cout << std::format("LobbyManager::HandleRequestCreateSession: session {} is created.\n", res);
            response.set_sessionindex(res);
            response.set_reason("ok");
        }
        else
        {
            response.set_sessionindex(-1);
            response.set_reason(reason);
        }
    }
    else
    {
        response.set_sessionindex(-1);
        response.set_reason("Parsing message error.");
    }

    std::string serial{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_SESSION_CREATION),
        static_cast<int>(serial.length()) + 12);
    t.insert(t.end(), serial.begin(), serial.end());

    client->PostWrite(t);
}

void LobbyManager::HandleRequestSessionEnter(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    M_RequestSessionEntry res;
    M_ResponseSessionEntry response;

    if (res.ParseFromArray(serializedData, length))
    {
        int sessionIndex = res.sessionindex();
        bool success = false;
        std::string reason;

        std::scoped_lock sl{_emptySessionsMtx, _sessionsMtx};

        // 비어있는 session(host가 없는) index
        if (_emptySessions.find(sessionIndex) != _emptySessions.end())
        {
            auto ss = _emptySessions[sessionIndex];
            success = ss->AddClient(client, reason);

            response.set_result(success);
            response.set_reason(reason);
        }
        else if (_sessions.find(sessionIndex) != _sessions.end())
        {
            auto ss = _sessions[sessionIndex];
            success = ss->AddClient(client, reason);

            std::cout << std::format("client({}) added to session({}). {}\n",
                                     client->GetToken(), ss->GetSessionToken(), reason);

            response.set_result(success);
            response.set_reason(reason);
        }
        else
        {
            response.set_result(false);
            response.set_reason("Invalid session index");
        }
    }
    else
    {
        response.set_result(false);
        response.set_reason("Parsing message error");
    }

    std::string serial{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_SESSION_ENTRY),
        static_cast<int>(serial.length()) + 12);
    t.insert(t.end(), serial.begin(), serial.end());

    client->PostWrite(t);
}

void LobbyManager::HandleRequestSessionLeave(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    auto clientToken = client->GetToken();

    client->Stop();

    std::scoped_lock sl{_loginedClientMtx};

    _loginedClients.erase(clientToken);
}

void LobbyManager::HandleRequestLogout(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
}

void LobbyManager::ForwardUDPData(char *data, int length)
{
}

void LobbyManager::StartRefreshSessionCache()
{
    RewriteSessionListCache();
}