#pragma once
#include "ClientSocket.h"
#include <boost/asio.hpp>
#include <list>
#include <mutex>
#include <map>
#include "protoc/proto_message.pb.h"
#include <queue>
#include <vector>

struct ObjectTransform
{
    int _index = 0;
    float _x = 0.f, _y = 0.f, _z = 0.f, _r = 0.f;
};

class Room : std::enable_shared_from_this<Room>
{
    enum class UDPBufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096,
        SEND_BUFFER_SIZE = 4096
    };

    int _roomIndex;
    boost::asio::io_context &_io;
    boost::asio::steady_timer _timer;
    boost::asio::ip::udp::socket &_udpSocket;
    boost::asio::ip::udp::endpoint _udpRemoteEndpoint;

    std::list<std::shared_ptr<ClientSocket>> _clients;
    std::mutex _mtxClient;

    std::map<int, boost::asio::ip::udp::endpoint> _clientUDPEndpoint;
    std::mutex _mtxClientUDPEndpoint;

    PROTO_SyncCharacterPhysics _scp;
    std::mutex _mtxSCP;
    
    std::queue<std::vector<char>> _sendQueue;
    std::mutex _mtxSendQueue;

    std::shared_ptr<char[]> _udpRecvBuffer;
    
    const float _syncRateMs = 0.02f;

private:
    void StartTimer();
    void SendUDPData(boost::asio::ip::udp::endpoint remoteEndpoint);

public:
    Room(boost::asio::io_context &io,
        boost::asio::ip::udp::socket &socket);
    void EnterRoom(std::shared_ptr<ClientSocket> client);
    void ExitRoom(std::shared_ptr<ClientSocket> client);
    void ReportClientTransform(
        PROTO_ObjectTransform *ot,
        boost::asio::ip::udp::endpoint sender);
    void PrintStatus();
    ~Room();
    // void SetTransfrom(int index, )
};