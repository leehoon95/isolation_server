#include "lobbyManager.h"
#include "util.h"
#include "isolation_pb/lobby_message.pb.h"
#include "redisService.h"

using namespace boost;

LobbyManager::LobbyManager(asio::io_context &io, asio::ip::udp::socket &socket)
    : _io(io),
      _udpSocket(socket)
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

    auto keyUser = std::format("user:{}", client->GetToken());
    bool keyExists = rs.Exists(keyUser); // nickname 중복검사

    if (keyExists)
    {
        reason = "This nickname is already exists";
        return false;
    }

    _loginedClients[client->GetToken()] = client;
    rs.Set(keyUser, nickname);
    rs.Expire(keyUser, 5);

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
            if (auto c = wc.lock())
            {
                if (auto lm = wlm.lock())
                {
                    lm->SendCurrentSessionList(c);
                }
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_CREATE_SESSION,
        [wlm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto lm = wlm.lock())
                {
                    lm->HandleRequestCreateSession(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_SESSION_ENTER,
        [wlm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto lm = wlm.lock())
                {
                    lm->HandleRequestEnterToSession(c, serializedData, length);
                }
            }
        });

    client->SetPacketHandler(
        LobbyMessage_Type::REQUEST_SESSION_LEAVE,
        [wlm, wc](char *serializedData, int length)
        {
            if (auto c = wc.lock())
            {
                if (auto lm = wlm.lock())
                {
                    lm->HandleRequestLeaveFromSession(c, serializedData, length);
                }
            }
        });

    client->SetDisconnectHandler(
        LobbyMessage_Type::LBMT_DISCONNECTED,
        [wlm, wc](system::error_code &ec)
        {
            if (auto c = wc.lock())
            {
                if (auto lm = wlm.lock())
                {
                    lm->DisconnectedClient(c);
                }
            }
        });
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

void LobbyManager::SendCurrentSessionList(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock<std::mutex> sl{_sessionListCacheMtx};

    M_ResponseSessionList rsl;

    if (_sessionListCache.empty())
    {
        rsl.set_count(0);
    }
    else
    {
        rsl.set_count(static_cast<int>(_sessionListCache.size()));

        for (const auto &entry : _sessionListCache)
        {
            M_SessionInfo *si = rsl.add_list();
            si->set_sessionindex(entry.second.first);
            si->set_sessionname(entry.second.second);
        }
    }

    std::string rrlString{rsl.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_SESSION_LIST),
        static_cast<int>(rrlString.length()) + 12);
    t.insert(t.end(), rrlString.begin(), rrlString.end());

    client->PostWrite(t);
}

int LobbyManager::CreateSession(
    const uint64_t hostToken,
    const std::string_view name,
    const std::string_view password,
    std::string &reason)
{
    auto session = std::make_shared<Session>();

    bool res = session->CreateSession(
        hostToken,
        4,
        name,
        password,
        reason);

    if (!res)
        return -1;

    {
        std::scoped_lock<std::mutex> sl{_sessionsMtx};
        ++_sessionIndexCounter;
        _sessions[_sessionIndexCounter] = session;

        session->SetReceiveJoincodeCallback([wlobby = std::weak_ptr<LobbyManager>(shared_from_this()),
                                             ssindex = _sessionIndexCounter]()
                                            {
        if (auto l = wlobby.lock()) {
               l->ActivateSession(ssindex);
        } });
    }

    reason = "ok";

    return _sessionIndexCounter;
}

void LobbyManager::ActivateSession(int sessionIndex)
{
    std::scoped_lock sl(_sessionsMtx, _activatedSessionsMtx);

    if (_sessions.find(sessionIndex) == _sessions.end())
        return;

    auto ss = _sessions[sessionIndex];
    _activatedSessions[sessionIndex] = ss;
    _sessions.erase(sessionIndex);
}

void LobbyManager::HandleRequestCreateSession(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    M_RequestCreateSession rcs;
    M_ResponseCreateSession response;

    if (rcs.ParseFromArray(serializedData, length))
    {
        std::string name{rcs.sessionname()};
        std::string password{rcs.password()};

        std::string reason;
        int res = CreateSession(client->GetToken(), name, password, reason);

        if (res >= 0)
        {
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

    std::string rcrString{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_CREATE_SESSION),
        static_cast<int>(rcrString.length()) + 12);

    client->PostWrite(t);
}

void LobbyManager::HandleRequestEnterToSession(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
    M_RequestEnterSession res;
    M_ResponseEnterSession response;

    if (res.ParseFromArray(serializedData, length))
    {
        int sessionIndex = res.sessionindex();
        bool success = false;
        std::string reason;

        if (res.host())
        {
            if (_sessions.find(sessionIndex) == _sessions.end())
            {
                response.set_reason("Invalid session index");
                response.set_result(false);
            }
            else
            {
                auto ss = _sessions[sessionIndex];
                success = ss->AddClient(client, true, reason);
            }
        }
        else
        {
            if (_activatedSessions.find(sessionIndex) == _sessions.end())
            {
                response.set_reason("Invalid session index");
                response.set_result(false);
            }
            else
            {
                auto ss = _activatedSessions[sessionIndex];
                success = ss->AddClient(client, false, reason);
            }
        }

        if (success)
        {
            response.set_result(true);
            response.set_reason("ok");
        }
        else
        {
            response.set_result(false);
            response.set_reason(reason);
        }
    }
    else
    {
        response.set_result(false);
        response.set_reason("Parsing message error");
    }

    std::string rcrString{response.SerializeAsString()};
    std::vector<char> t;

    append_prot_packet(
        t,
        static_cast<int>(LobbyMessage_Type::RESPONSE_CREATE_SESSION),
        static_cast<int>(rcrString.length()) + 12);

    client->PostWrite(t);
}

void LobbyManager::HandleRequestLeaveFromSession(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
}

void LobbyManager::HandleRequestLogout(std::shared_ptr<ClientSocket> client, char *serializedData, int length)
{
}

void LobbyManager::ForwardUDPData(char *data, int length)
{
}