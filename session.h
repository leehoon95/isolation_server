#pragma once
#include <map>
#include <memory>
#include <string>
#include <functional>

class ClientSocket;
class LobbyManager;

/*
    sever와 redis 간 통신 및 동기화 책임지는 클래스
    session과 관련된 redis 통신은 반드시 Session 객체를 통할 것
*/
class Session : public std::enable_shared_from_this<Session>
{
    uint64_t _token; // session token
    std::shared_ptr<ClientSocket> _host;
    std::map<uint64_t, std::shared_ptr<ClientSocket>> _clients; // token, client
    std::weak_ptr<LobbyManager> _lobbyManager;
    std::string _sessionKey;
    std::string _sessionClientsKey;
    std::string _joincodeCache;

    std::function<void(void)> _receiveJoincodeCallback;

private:
    Session(Session &) = delete;
    Session &operator=(const Session &) = delete;
    void SetJoincode(const std::string &joincode);
    std::string GetJoincode();
    bool IsHost(uint64_t token)
    {
        return _host && _host->GetToken() == token;
    }
    void CallReceiveJoincodeCallback()
    {
        if (_receiveJoincodeCallback)
            _receiveJoincodeCallback();
    }

public:
    explicit Session();
    bool CreateSession(
        const uint64_t hostToken,
        const unsigned int maxClientCount,
        std::string_view name,
        std::string_view password,
        std::string &reason);
    void SetReceiveJoincodeCallback(std::function<void(void)> callback);
    bool AddClient(
        std::shared_ptr<ClientSocket> client,
        bool host,
        std::string &reason);
    void StopSession();
    virtual ~Session();
};