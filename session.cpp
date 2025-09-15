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

void Session::SetJoincode(const std::string &joincode)
{
    std::string sessionKey{std::format("session:{}", _token)};

    RS &rs = RS::Instance();

    rs.HashSet(sessionKey, "joincode", joincode);
}

std::string Session::GetJoincode()
{
    std::string sessionKey{std::format("session:{}", _token)};

    RS &rs = RS::Instance();

    auto res = rs.HashGet(sessionKey, "joincode");

    return res.value_or("");
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
    _sessionKey = std::format("session:{}", hostToken);

    if (rs.Exists(_sessionKey))
    {
        ASSERT(false, "Session::CreateSession: session key already exists.");
        return false;
    }

    /*
        session 설정의 의미
        hostToken: session 생성 요청 client token
        maxClientCount: session에 입장 가능한 최대 client 수
        name: session 리스트에 보이는 이름(host가 입장하기 전까지는 보이지 않음)
        password: session 입장시 필요한 비밀번호(필수 아님)

        host는 1분 안에 접속해야 함
    */
    rs.HashSet(_sessionKey, {{"hostToken", std::to_string(hostToken)},
                             {"maxClientCount", std::to_string(maxClientCount)},
                             {"name", std::string{name}},
                             {"password", std::string{password}},
                             {"joincode", ""}});
    rs.Expire(_sessionKey, 5);

    _sessionClientsKey = std::format("session:{}:clients", hostToken);
    rs.Expire(_sessionClientsKey, 5);

    return true;
}

void Session::SetReceiveJoincodeCallback(std::function<void(void)> callback)
{
    _receiveJoincodeCallback = callback;
}

bool Session::AddClient(
    std::shared_ptr<ClientSocket> client,
    bool host,
    std::string &reason)
{
    if (!client)
    {
        ASSERT(false, "Session::TryEnterAsClient: client is null.\n");
        return false;
    }

    if (_host == nullptr)
    {
        reason = "Session host is not set";
        return false;
    }

    auto &rs = RS::Instance();

    if (!rs.Exists(_sessionKey))
    {
        reason = "Session does not exist";
        return false;
    }

    uint64_t token = client->GetToken();

    if (host && _host->GetToken() != token)
    {
        reason = "Host token does not match";
        return false;
    }

    rs.HashSet(_sessionClientsKey, std::to_string(token), client->GetNickname());

    std::weak_ptr<Session> wss{shared_from_this()};
    std::weak_ptr<ClientSocket> wc{std::weak_ptr<ClientSocket>{client}};

    // host -> session. The joincode to open this session.
    if (host)
    {
        client->SetPacketHandler(
            GameMessage_Type::JOINCODE,
            [wss, wc](char *serializedData, int length)
            {
                if (auto ss = wss.lock())
                {
                    M_Joincode j;

                    if (j.ParseFromArray(serializedData, length))
                    {
                        std::cout << std::format("Session::AddClient. JOINCODE: {}\n", j.joincode());
                        if (ss->IsHost(j.sendertoken()))
                        {
                            std::cout << std::format("Session::AddClient. joincode is set\n");
                            ss->SetJoincode(j.joincode());
                            ss->CallReceiveJoincodeCallback();
                        }
                    }
                }
            });
    }

    // client -> session. Request the joincode to join this session.
    client->SetPacketHandler(
        GameMessage_Type::REQUEST_JOINCODE,
        [wss, wc](char *serializedData, int length)
        {
            if (auto ss = wss.lock())
            {
                if (auto c = wc.lock())
                {
                    if (auto c = wc.lock())
                    {
                        M_RequestJoincode rj;

                        if (rj.ParseFromArray(serializedData, length))
                        {
                            M_Joincode response;

                            auto joincode{ss->GetJoincode()};

                            if (joincode.empty())
                            {
                                ASSERT(false, "Session::AddClient REQUEST_JOINCODE: Joincode is empty.\n");
                                return;
                            }

                            response.set_joincode(joincode);

                            auto jString{response.SerializeAsString()};
                            std::vector<char> t;

                            append_prot_packet(
                                t,
                                static_cast<int>(GameMessage_Type::JOINCODE),
                                static_cast<int>(jString.length()) + 12);

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

    if (host)
    {
        M_RequestJoincode request;
        std::string rjString{request.SerializeAsString()};
        std::vector<char> t;

        append_prot_packet(
            t,
            static_cast<int>(GameMessage_Type::REQUEST_JOINCODE),
            static_cast<int>(rjString.length()) + 12);

        client->PostWrite(t);
    }

    return true;
}

void Session::StopSession()
{
}

Session::~Session()
{
    TokenPool64::Instance().release(_token);
}