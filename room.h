#pragma once
#include "ClientSocket.h"
#include <boost/asio.hpp>
#include <list>
#include <mutex>
#include <map>
#include <queue>
#include <vector>

struct ObjectTransform
{
    int _index = 0;
    float _x = 0.f, _y = 0.f, _z = 0.f, _r = 0.f;
};

class Room : public std::enable_shared_from_this<Room>
{
    enum class UDPBufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096,
        SEND_BUFFER_SIZE = 4096
    };

    enum class BroadcastingInterval : int 
    {
        INTERVAL = 100
    };

    int _index;
    std::string _name;

    boost::asio::io_context &_io;
    boost::asio::steady_timer _timer;
    boost::asio::ip::udp::socket &_udpSocket;
    boost::asio::ip::udp::endpoint _udpRemoteEndpoint;

    std::map<int, std::shared_ptr<ClientSocket>> _clients;
    std::mutex _mtxClient;

    std::map<int, boost::asio::ip::udp::endpoint> _clientUDPEndpoint;
    std::mutex _mtxClientUDPEndpoint;

    //std::map<int, std::shared_ptr<PM_ObjectTransform>> _transforms;
    std::mutex _mtxTransforms;

    //PM_SyncCharacterPhysics _scp;
    std::mutex _mtxSCP;

    std::queue<std::vector<char>> _sendQueue;
    std::mutex _mtxSendQueue;

    std::shared_ptr<char[]> _udpRecvBuffer;

    const float _syncRateMs = 0.02f;

private:
    void StartTimer();
    void SendUDPData(
        boost::asio::ip::udp::endpoint remoteEndpoint,
        std::shared_ptr<std::vector<char>> buffer);

public:
    Room(boost::asio::io_context &io,
         boost::asio::ip::udp::socket &socket);
    void EnterRoom(std::shared_ptr<ClientSocket> client);
    void ExitRoom(const int index);
    // void ReportClientTransform(
    //     PM_ObjectTransform *ot,
    //     boost::asio::ip::udp::endpoint sender);
    void SetIndex(int index) { _index = index; }
    void SetName(const std::string &name) { _name = name; }
    int GetIndex() const { return _index; }
    std::string GetName() const { return _name; }
    void PrintStatus();
    ~Room();
    // void SetTransfrom(int index, )
};