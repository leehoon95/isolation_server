#include <boost/asio.hpp>
#include "ClientSocket.h"
#include <map>
#include <list>

// class Room : std::enable_shared_from_this<Room>
// {
//     std::list<std::shared_ptr<ClientSocket>> _clients;

// public:
//     void EnterRoom(std::shared_ptr<ClientSocket> client);

// };

class Server
{
    boost::asio::io_context &_io;
    // ClientSocket _client;
    std::mutex _connMtx;
    std::map<unsigned int, std::shared_ptr<ClientSocket>> _connectedClients;
    std::mutex _loginedMtx;
    std::map<std::string, std::shared_ptr<ClientSocket>> _loginedClients;
    unsigned int _clientIndex = 0;

public:
    explicit Server(
        boost::asio::io_context &io);

    void Stop();
    void AddClient(std::shared_ptr<ClientSocket> client);
    void PrintStatus();
};
