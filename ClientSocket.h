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
    uint64_t _token;
    std::shared_ptr<char[]> _recvBuffer;

    std::deque<std::shared_ptr<std::vector<char>>> _writeBufferQueue;
    std::atomic<bool> _writeInProcessing;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    std::mutex _writeBufferMtx;

    std::map<int, std::function<void(char *, int)>> _packetHandler;
    std::mutex _packetHandlerMtx;

    std::map<int, std::function<void(boost::system::error_code&)>> _disconnectHandler;
    std::mutex _disconnectHandlerMtx;
    // boost::system::error_code _lastError;
    bool _isConnected;
    bool _stopped;

    // std::function<void(std::shared_ptr<ClientSocket>)> _disconnectHandler;

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
    void HandleError(boost::system::error_code &ec);
    void HandleDisconnect(boost::system::error_code &ec);
    std::shared_ptr<char[]> GetReceiveBuffer() { return _recvBuffer; };

public:
    explicit ClientSocket(
        boost::asio::io_context &io,
        boost::asio::ip::tcp::socket socket);
    bool Init(int index);
    void Stop();
    bool PostWrite(std::vector<char> &data);
    void SetMessageDeserializer(std::function<void(char *, int)> dispatcher);
    void SetPacketHandler(int type, std::function<void(char *, int)> handler);
    void SetDisconnectHandler(int type, std::function<void(boost::system::error_code&)> handler);
    void RemovePacketHandler(int type);
    void RemoveDisconnectHandler(int type);
    void ClearPacketHandler();
    void ClearDisconnectHandler();
    void SetNickname(std::string nickname);
    void SetToken(uint64_t token) { _token = token; }
    uint64_t GetToken() { return _token; }
    unsigned int GetIndex() { return _index; }
    std::string GetNickname() { return _nickname; }

    ~ClientSocket();
};