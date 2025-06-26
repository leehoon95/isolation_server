#pragma once
#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>
#include "room.h"

class Server
{
    enum class UDPBufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096,
        SEND_BUFFER_SIZE = 4096
    };

    boost::asio::io_context &_io;
    boost::asio::ip::udp::socket _udpSocket;
    //boost::asio::ip::udp::socket _udpSocket2;
    boost::asio::ip::udp::endpoint _remoteEndpoint;
    std::shared_ptr<char[]> _udpRecvBuffer;
    std::shared_ptr<char[]> _udpSendBuffer;
    std::mutex _connMtx;
    std::map<unsigned int, std::shared_ptr<ClientSocket>> _connectedClients;
    std::mutex _loginedMtx;
    std::map<std::string, std::shared_ptr<ClientSocket>> _loginedClients;
    Room _room;

    unsigned int _clientIndex = 0;

private:
    void StartUDPReceive();

public:
    explicit Server(
        boost::asio::io_context &io);

    void Stop();
    void AddClient(std::shared_ptr<ClientSocket> client);
    void PrintStatus();
};
