#include "session.h"
#include <iostream>
#include "util.h"
#include "isolation_pb/session_message.pb.h"
// #include "isolation_pb/lobby_message.pb.h"
#include "ClientSocket.h"
#include "lobbyManager.h"
#include <boost/asio.hpp>
#include "redisService.h"
#include "tokenPool.h"
#include <format>

using namespace boost;

Session::Session()
    : _token(0)
{
    // ASSERT(hostToken != 0, "Session::Session: hostToken is 0.");
    _token = TokenPool64::Instance().allocate();
}

bool Session::SetHostJoinCode(
    std::shared_ptr<ClientSocket> sender,
    const std::string &joinCode)
{
    RS &rs = RS::Instance();

    auto ht = rs.HashGet(_sessionKey, "hostToken");

    if (!ht)
    {
        std::cerr << "Session::SetHostJoinCode: session does not have hostToken\n";
        return false;
    }

    uint64_t hostToken = std::stoull(*ht);
    if (hostToken != sender->GetToken())
    {
        return false;
    }

    rs.HashSet(_sessionKey, "joinCode", joinCode);

    return true;
}

std::string Session::GetJoinCode()
{
    RS &rs = RS::Instance();

    auto res = rs.HashGet(_sessionKey, "joinCode");

    return res.value_or("");
}

bool Session::IsValidSession(std::string &reason)
{
    auto &rs = RS::Instance();

    if (!rs.Exists(_sessionKey))
    {
        reason = std::format("Session::IsValidSession. Session key({}) does not exist", _sessionKey);
        std::cerr << reason << std::endl;
        return false;
    }

    if (!rs.HashFieldExists(_sessionKey, "hostToken") ||
        !rs.HashFieldExists(_sessionKey, "maxClientCount") ||
        !rs.HashFieldExists(_sessionKey, "name") ||
        !rs.HashFieldExists(_sessionKey, "password") ||
        !rs.HashFieldExists(_sessionKey, "joinCode"))
    {
        reason = std::format("Session::IsValidSession. This session({}) is incomplete.", _sessionKey);
        std::cerr << reason << std::endl;
        return false;
    }

    auto hostToken = rs.HashGet(_sessionKey, "hostToken");
    if (!hostToken)
    {
        reason = "Session::IsValidSession. host token doesn't exists in session key.";
        std::cerr << reason << std::endl;
        return false;
    }

    try
    {
        auto tokenNum = std::stoull(*hostToken);

        if (tokenNum == 0)
        {
            reason = std::format("Session::IsValidSession. hostToken is 0");
            std::cerr << reason << std::endl;
            return false;
        }
    }
    catch (std::invalid_argument &e)
    {
        reason = std::format("Session::IsValidSession. hostToken is invalid {}. what: {}",
                             *hostToken, e.what());
        std::cerr << reason << std::endl;
        return false;
    }
    catch (std::out_of_range &e)
    {
        reason = std::format("Session::IsValidSession. hostToken is out of range {}. what: {}",
                             *hostToken, e.what());
        std::cerr << reason << std::endl;
        return false;
    }

    if (!rs.Exists(_sessionClientsKey))
    {
        reason = std::format("Session::IsValidSession. Session clients key({}) does not exist", _sessionClientsKey);
        std::cerr << reason << std::endl;
        return false;
    }

    return true;
}

bool Session::IsActiveSession()
{
    auto &rs = RS::Instance();

    auto hostToken = rs.HashGet(_sessionKey, "hostToken");
    if (!hostToken)
    {
        std::cerr << "Session::IsActiveSession. host token doesn't exists in session key.\n";
        return false;
    }

    if (!rs.HashFieldExists(_sessionClientsKey, *hostToken))
    {
        std::cerr << "Session::IsActiveSession. host token doesn't exists in session clients key.\n";
        return false;
    }

    return true;
}

bool Session::CreateSession(
    std::shared_ptr<ClientSocket> client,
    const unsigned int maxClientCount,
    std::string_view name,
    std::string_view password,
    std::string &reason)
{
    if (!client)
    {
        reason = "client is invalid";
        return false;
    }

    if (maxClientCount == 0)
    {
        reason = "max client count is 0";
        return false;
    }

    if (utf8_length(name) < 2)
    {
        reason = "The session name is too short";
        return false;
    }

    auto &rs = RS::Instance();
    std::string clientToken{std::to_string(client->GetToken())};

    // token이 중복을 허용하지 않으므로 session key도 그러함
    _sessionKey = std::format("session:{}", _token);

    /*
        session 설정의 의미
        hostToken: session 생성 요청 client token
        maxClientCount: session에 입장 가능한 최대 client 수
        name: session 리스트에 보이는 이름(host가 입장하기 전까지는 보이지 않음)
        password: session 입장시 필요한 비밀번호(필수 아님)
        joinCode: client가 host 접속시 필요한 코드

        host는 3분 안에 접속해야 함
    */
    rs.HashSet(_sessionKey, {{"hostToken", clientToken},
                             {"maxClientCount", std::to_string(maxClientCount)},
                             {"name", std::string{name}},
                             {"password", std::string{password}},
                             {"joinCode", ""}});

    _sessionClientsKey = std::format("session:{}:clients", _token);
    rs.HashSet(_sessionClientsKey, clientToken, client->GetNickname());

    rs.Expire(_sessionKey, 3);
    rs.Expire(_sessionClientsKey, 3);

    return true;
}

void Session::SetReceiveJoinCodeCallback(std::function<void(void)> callback)
{
    _receiveJoinCodeCallback = callback;
}

bool Session::AddClient(
    std::shared_ptr<ClientSocket> client,
    std::string &reason)
{
    std::scoped_lock sl{_addClientMtx};

    if (!client)
    {
        ASSERT(false, "Session::AddClient. client is null.\n");
        return false;
    }

    auto &rs = RS::Instance();

    std::string userKey{std::format("client:{}", client->GetToken())};

    if (!rs.Exists(userKey))
    {
        reason = std::format("User key({}) does not exist", _sessionKey);
        std::cerr << std::format("Session::AddClient. {}\n", userKey);
        return false;
    }

    if (!IsValidSession(reason))
    {
        return false;
    }

    uint64_t clientToken = client->GetToken();

    auto hostToken = rs.HashGet(_sessionKey, "hostToken");

    bool host = std::stoull(*hostToken) == clientToken;

    std::weak_ptr<Session> wss{shared_from_this()};
    std::weak_ptr<ClientSocket> wc{std::weak_ptr<ClientSocket>{client}};

    if (host)
    {
        /*
            host가 joinCode를 알림
            _receiveJoinCodeCallback을 호출해서 session를 active함(client가 들어올 수 있도록)
        */
        client->SetPacketHandler(
            SessionMessage_Type::JOINCODE,
            [wss, wc](char *serializedData, int length)
            {
                auto ss = wss.lock();
                auto c = wc.lock();

                if (ss && c)
                {
                    M_JoinCode j;

                    if (j.ParseFromArray(serializedData, length))
                    {
                        if (j.joincode().empty())
                        {
                            std::cout << "Session::AddClient. Received joinCode. but empty.\n";
                        }

                        std::cout << std::format("Session::AddClient. JOINCODE: {}\n", j.joincode());
                        if (ss->SetHostJoinCode(c, j.joincode()))
                        {
                            std::cout << std::format("Session::AddClient. joinCode is set\n");
                            ss->OnReceiveHostJoinCode();
                            // LobbyManager will activate this session
                        }
                        else
                        {
                            std::cerr << "Session::AddClient. JOINCODE: Failed to set joinCode\n";
                        }
                    }
                }
            });

        rs.Persist(_sessionKey);
        rs.Persist(_sessionClientsKey);
    }
    else
    {
        if (!rs.HashGet(_sessionClientsKey, *hostToken))
        {
            reason = "Session::AddClient. The host client is not in session.";
            std::cerr << reason << std::endl;
            return false;
        }

        /*
            mcc=max client count
            session이 full인지 검사
        */
        auto mccOpt = rs.HashGet(_sessionKey, "maxClientCount");
        if (!mccOpt)
        {
            reason = "Invalid session";
            std::cerr << "Seesion::AddClient. maxClientCount is null\n";
            return false;
        }

        auto mccStr = std::move(*mccOpt);
        auto mcc = std::stoull(mccStr);

        auto currentClientCount = rs.HashLen(_sessionClientsKey);

        if (currentClientCount > mcc)
        {
            reason = "Session is full";
            std::cerr << "Session::AddClient. Session is full.\n";
            return false;
        }
    }

    client->SetPacketHandler(
        SessionMessage_Type::REQUEST_ENVIRONMENT,
        [wc, sessionKey = _sessionKey, host](char *serializedData, int length)
        {
            auto c = wc.lock();

            if (c)
            {
                auto &rs = RS::Instance();

                M_Environment env;

                env.set_sessionname(*rs.HashGet(sessionKey, "name"));
                env.set_joincode(*rs.HashGet(sessionKey, "joinCode"));
                env.set_password(*rs.HashGet(sessionKey, "password"));
                env.set_host(host);

                auto serial{env.SerializeAsString()};
                std::vector<char> t;

                append_prot_packet(
                    t,
                    static_cast<int>(SessionMessage_Type::ENVIRONMENT),
                    static_cast<int>(serial.length()) + 12);
                t.insert(t.end(), serial.begin(), serial.end());

                c->PostWrite(t);
            }
        });

    rs.HashSet(_sessionClientsKey, std::to_string(clientToken), client->GetNickname());
    reason = "ok";
        
    return true;
}

bool Session::GetSessionInfo(
    std::string &name,
    int &maxClientCount,
    int &clientCount,
    std::string &password,
    std::string &joinCode)
{
    std::scoped_lock sl{_getSessionInfoMtx};

    std::string reason;
    if (!IsValidSession(reason))
    {
        std::cerr << std::format("Session::GetSessionInfo. Invalid session. {}\n", reason);
        return false;
    }

    auto &rs = RS::Instance();

    clientCount = rs.HashLen(_sessionClientsKey);
    if (clientCount == 0)
    {
        std::cerr << std::format("Session::GetSessionInfo. This session is inactive({}).",
                                 _sessionKey);
        return false;
    }

    auto res = rs.HashGet(_sessionKey, "name");
    if (res)
        name = *res;
    else
    {
        std::cerr << std::format("Session::GetSessionInfo. name is null in activated session({}).",
                                 _sessionKey);
        return false;
    }

    res = rs.HashGet(_sessionKey, "maxClientCount");
    if (res)
        maxClientCount = std::stoi(*res);
    else
    {

        return false;
    }

    res = rs.HashGet(_sessionKey, "password");
    if (res)
        password = *res;
    else
    {
        return false;
    }

    res = rs.HashGet(_sessionKey, "joinCode");
    if (res)
        joinCode = *res;
    else
    {
        return false;
    }

    return true;
}

void Session::StopSession()
{
    auto &rs = RS::Instance();
    rs.Del(_sessionKey);
    rs.Del(_sessionClientsKey);
}

Session::~Session()
{
    TokenPool64::Instance().release(_token);
    StopSession();
}