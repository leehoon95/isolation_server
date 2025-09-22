#include "session.h"
#include <iostream>
#include "util.h"
#include "isolation_pb/game_message.pb.h"
#include "isolation_pb/lobby_message.pb.h"
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
        reason = std::format("Session key({}) does not exist", _sessionKey);
        std::cerr << "Session::IsValidSession: session key does not exists.\n";
        return false;
    }

    if (!rs.Exists(_sessionClientsKey))
    {
        reason = std::format("Session clients key({}) does not exist", _sessionClientsKey);
        std::cerr << "Session::IsValidSession: session clients key does not exists.\n";
        return false;
    }
}

bool Session::CreateSession(
    const uint64_t hostToken,
    const unsigned int maxClientCount,
    std::string_view name,
    std::string_view password,
    std::string &reason)
{
    // ASSERT(hostToken != 0, "Session::CreateSession: hostToken is 0.");
    // ASSERT(maxClientCount > 0, "Session::CreateSession: maxClientCount is 0.");
    // ASSERT(!name.empty(), "Session::CreateSession: name is empty.");

    if (hostToken == 0)
    {
        reason = "Host token is 0";
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
    rs.HashSet(_sessionKey, {{"hostToken", std::to_string(hostToken)},
                             {"maxClientCount", std::to_string(maxClientCount)},
                             {"name", std::string{name}},
                             {"password", std::string{password}},
                             {"joinCode", ""}});
    rs.Expire(_sessionKey, 3);

    _sessionClientsKey = std::format("session:{}:clients", _token);
    rs.Expire(_sessionClientsKey, 3);

    return true;
}

void Session::SetReceiveJoinCodeCallback(std::function<void(void)> callback)
{
    _receiveJoinCodeCallback = callback;
}

bool Session::AddClient(
    std::shared_ptr<ClientSocket> client,
    bool host,
    std::string &reason)
{
    std::scoped_lock sl{_addClientMtx};

    if (!client)
    {
        ASSERT(false, "Session::TryEnterAsClient: client is null.\n");
        return false;
    }

    auto &rs = RS::Instance();

    std::string userKey{std::format("user:{}", client->GetToken())};

    if (!rs.Exists(userKey))
    {
        reason = std::format("User key({}) does not exist", _sessionKey);
        std::cerr << std::format("Session::AddClient: {}\n", userKey);
        return false;
    }

    if (!IsValidSession(reason))
    {
        return false;
    }

    uint64_t clientToken = client->GetToken();

    /*
        session을 생성한 hostToken과 host로 들어온 client가 맞는지 검사
    */
    if (host)
    {
        auto ht = rs.HashGet(_sessionKey, "hostToken");

        if (!ht)
        {
            reason = "Session does not have hostToken";
            // std::cerr << "Session::AddClient: session does not have hostToken\n";
            return false;
        }

        uint64_t hostToken = std::stoull(*ht);

        if (hostToken != clientToken)
        {
            reason = "Host token does not match";
            return false;
        }
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

    std::weak_ptr<Session> wss{shared_from_this()};
    std::weak_ptr<ClientSocket> wc{std::weak_ptr<ClientSocket>{client}};

    // host -> session. The joinCode to open this session.
    if (host)
    {
        client->SetPacketHandler(
            LobbyMessage_Type::JOINCODE,
            [wss, wc](char *serializedData, int length)
            {
                if (auto ss = wss.lock())
                {
                    M_JoinCode j;

                    if (auto c = wc.lock())
                    {
                        if (j.ParseFromArray(serializedData, length))
                        {
                            std::cout << std::format("Session::AddClient. JOINCODE: {}\n", j.joincode());
                            if (ss->SetHostJoinCode(c, j.joincode()))
                            {
                                std::cout << std::format("Session::AddClient. joinCode is set\n");
                                ss->CallReceiveJoinCodeCallback();
                            }
                            else
                            {
                                std::cerr << "Session::AddClient. JOINCODE: Failed to set joinCode\n";
                            }
                        }
                    }
                }
            });

        rs.Persist(_sessionKey);
        rs.Persist(_sessionClientsKey);
    }
    else
    {
        // client -> session. Request the joinCode to join this session.
        client->SetPacketHandler(
            LobbyMessage_Type::REQUEST_JOINCODE,
            [wss, wc](char *serializedData, int length)
            {
                if (auto ss = wss.lock())
                {
                    if (auto c = wc.lock())
                    {
                        if (auto c = wc.lock())
                        {
                            M_RequestJoinCode rj;

                            if (rj.ParseFromArray(serializedData, length))
                            {
                                M_JoinCode response;

                                auto joinCode{ss->GetJoinCode()};

                                if (joinCode.empty())
                                {
                                    std::cerr << "Session::AddClient. REQUEST_JOINCODE: JoinCode is empty.\n";
                                    return;
                                }

                                response.set_joincode(joinCode);

                                auto serial{response.SerializeAsString()};
                                std::vector<char> t;

                                append_prot_packet(
                                    t,
                                    static_cast<int>(LobbyMessage_Type::JOINCODE),
                                    static_cast<int>(serial.length()) + 12);
                                t.insert(t.end(), serial.begin(), serial.end());

                                c->PostWrite(t);
                            }
                            else
                            {
                                std::cout << "Session.RESPONSE_JOINCODE parsing error.\n";
                            }
                        }
                    }
                }
            });
    }

    rs.HashSet(_sessionClientsKey, std::to_string(clientToken), client->GetNickname());

    // if (host)
    // {
    //     M_RequestJoinCode request;
    //     std::string rjString{request.SerializeAsString()};
    //     std::vector<char> t;

    //     append_prot_packet(
    //         t,
    //         static_cast<int>(GameMessage_Type::REQUEST_JOINCODE),
    //         static_cast<int>(rjString.length()) + 12);

    //     client->PostWrite(t);
    // }

    return true;
}

void Session::GetSessionInfo(
    std::string &name,
    int maxUserCount,
    int userCount,
    std::string &password)
{
    
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