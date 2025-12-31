#pragma once
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>
#include "room.h"
#include "lobbyManager.h"

class Server : public std::enable_shared_from_this<Server>
{
    enum class UDPBufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096,
        // SEND_BUFFER_SIZE = 4096
    };

    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket _udpSocket;
    boost::asio::ip::udp::endpoint _remoteEndpoint;
    std::shared_ptr<char[]> _udpRecvBuffer;

    std::map<uint64_t, std::shared_ptr<ClientSocket>> _connectedClients;
    std::mutex _connMtx;

    // std::map<std::string, std::shared_ptr<ClientSocket>> _loginedClients;
    // std::map<int, std::shared_ptr<ClientSocket>> _loginedClients;
    // std::mutex _loginedMtx;

    std::shared_ptr<LobbyManager> _lm;

    //unsigned int _clientIndex = 0;

private:
    
    void RemoveClient(uint64_t token);
    //bool TryLogin(std::shared_ptr<ClientSocket> client, std::string &reason);
    void HandleRequestLogin(
        std::shared_ptr<ClientSocket> client, 
        char *serializedData, int length);
    void HandleRequestCreationAccount(
        std::shared_ptr<ClientSocket> client, 
        char *serializedData, int length);
    void HandlerRequestPlayerData(
        std::shared_ptr<ClientSocket> client, 
        char *serializedData, int length);

private:
    void ReceiveUDP();

public:
    explicit Server(
        boost::asio::io_context &io);

    void Stop();
    void AddClient(std::shared_ptr<ClientSocket> client);
   
    void PrintStatus();
    void StartUDPReceive();
};
