#pragma once
#include <map>
#include <memory>
#include <string>
#include <functional>
#include "ClientSocket.h"

class LobbyManager;

/*
    sever와 redis 간 통신 및 동기화 책임지는 클래스
    session과 관련된 redis 통신은 반드시 Session 객체를 통할 것
*/
class Session : public std::enable_shared_from_this<Session>
{
    uint64_t _token; // session token
    std::mutex _addClientMtx;
    std::map<uint64_t, std::shared_ptr<ClientSocket>> _clients; // token, client
    std::weak_ptr<LobbyManager> _lobbyManager;
    std::string _sessionKey;
    std::string _sessionClientsKey;

    std::function<void(void)> _receiveJoinCodeCallback;

private:
    Session(Session &) = delete;
    Session &operator=(const Session &) = delete;
    bool SetHostJoinCode(
        std::shared_ptr<ClientSocket> sender,
        const std::string &joinCode);
    std::string GetJoinCode();
    bool IsValidSession(std::string &reason);
    void CallReceiveJoinCodeCallback()
    {
        if (_receiveJoinCodeCallback)
            _receiveJoinCodeCallback();
    }

public:
    explicit Session();
    bool CreateSession(
        const uint64_t hostToken,
        const unsigned int maxClientCount,
        std::string_view name,
        std::string_view password,
        std::string &reason);
    void SetReceiveJoinCodeCallback(std::function<void(void)> callback);
    bool AddClient(
        std::shared_ptr<ClientSocket> client,
        bool host,
        std::string &reason);
    void GetSessionInfo(
        std::string &name,
        int maxUserCount,
        int userCount,
        std::string &password);
    void StopSession();
    uint64_t GetSessionToken() { return _token; }
    virtual ~Session();
};