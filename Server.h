#pragma once
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>
#include "room.h"
#include "roomManager.h"

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
    //std::shared_ptr<char[]> _udpSendBuffer;
    
    std::map<int, std::shared_ptr<ClientSocket>> _connectedClients;
    std::mutex _connMtx;

    
    //std::map<std::string, std::shared_ptr<ClientSocket>> _loginedClients;
    std::map<int, std::shared_ptr<ClientSocket>> _loginedClients;
    std::mutex _loginedMtx;

    RoomManager _rm;
    //Room _room;

    unsigned int _clientIndex = 0;

private:
    void ReceiveUDP();
    bool MoveClientToLogin(std::shared_ptr<ClientSocket> client, std::string nickname);
    void RemoveClient(int index);
    void PassClientToRoomManager(std::shared_ptr<ClientSocket> client);

public:
    explicit Server(
        boost::asio::io_context &io);

    void Stop();
    void AddClient(std::shared_ptr<ClientSocket> client);
    void StartUDPReceive();
    void PrintStatus();
};
