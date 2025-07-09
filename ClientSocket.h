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
    int _index;
    std::string _nickname;
    std::shared_ptr<char[]> _recvBuffer;

    std::deque<std::shared_ptr<std::vector<char>>> _writeBufferQueue;
    std::atomic<bool> _writeInProcessing;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    std::mutex _writeBufferMtx;

    std::map<int, std::function<void(char *, int)>> _packetHandler;
    std::mutex _packetHandlerMtx;

    std::function<void(int, std::string)> _onDisconnected;

private:
    ClientSocket(ClientSocket &) = delete;
    ClientSocket &operator=(const ClientSocket &) = delete;
    void ReadAsync();
    void WriteAsync();
    void PushWriteBuffer(std::shared_ptr<std::vector<char>> buffer);
    std::shared_ptr<std::vector<char>> GetFrontWriteBuffer();
    void PopFrontWriteBuffer();
    bool IsWriteBufferQueueEmpty();
    bool IsWriteProcessing();
    void SetWriteProcessing(bool value);
    bool HandlePacket(int type, char *data, int length);
    std::shared_ptr<char[]> GetReceiveBuffer() { return _recvBuffer; };
    void PrintSocketErorrEof();

public:
    explicit ClientSocket(
        boost::asio::io_context &io,
        boost::asio::ip::tcp::socket socket);
    bool Init(int index);
    void Stop();
    bool PostWrite(std::vector<char> &data);
    void SetMessageDeserializer(std::function<void(char *, int)> dispatcher);
    void SetPacketHandler(int type, std::function<void(char *, int)> proc);
    void RemoveHandler(int type);
    // void OnDisconnected(std::function<void(int, std::string)> callback);
    void ClearHandler();
    void SetNickname(std::string nickname);
    // void SetIndex(unsigned int index) { _index = index; }
    unsigned int GetIndex() { return _index; }
    std::string GetNickname() { return _nickname; }
    ~ClientSocket();
};