#pragma once
#include <queue>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <boost/asio.hpp>

class ClientSocket : public std::enable_shared_from_this<ClientSocket>
{
    enum class BufferSize : size_t
    {
        RECV_BUFFER_SIZE = 4096
    };

    boost::asio::io_context &_io;
    boost::asio::ip::tcp::socket _socket;
    unsigned int _index;
    std::string _nickname;
    std::shared_ptr<char[]> _recvBuffer;
    int _remainedLengthToReceive = 0;
    int _allDataSize = 0;
    std::deque<std::shared_ptr<std::vector<char>>> _writeBufferQueue;
    std::atomic<bool> _writeInProgress;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;

    std::map<int, std::function<void(char *, int)>> _procedure;
    std::mutex _procMtx;

    std::function<void(unsigned int, std::string)> _onDisconnected;

    ClientSocket(ClientSocket &) = delete;
    ClientSocket &operator=(const ClientSocket &) = delete;
    void ReadAsync();
    void WriteAsync();

public:
    explicit ClientSocket(
        boost::asio::io_context &io,
        boost::asio::ip::tcp::socket socket);
    bool Init(unsigned int index);
    void Stop();
    bool PostWrite(std::vector<char> &data);
    void SetMessageDeserializer(std::function<void(char *, int)> dispatcher);
    void SetProcedure(int type, std::function<void(char *, int)> proc);
    void OnDisconnected(std::function<void(unsigned int, std::string)> callback);
    void ClearProcedure();
    void SetNickname(std::string nickname);
    // void SetIndex(unsigned int index) { _index = index; }
    unsigned int GetIndex() { return _index; }
    std::string GetNickname() { return _nickname; }
    ~ClientSocket();
};