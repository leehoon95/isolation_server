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
    uint64_t _token;
    std::string _nickname;
    std::shared_ptr<char[]> _recvBuffer;

    std::deque<std::shared_ptr<std::vector<char>>> _writeBufferQueue;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    std::mutex _writeBufferMtx;

    std::map<int, std::function<void(char *, int)>> _packetHandler;
    std::mutex _packetHandlerMtx;

    std::map<int, std::function<void(boost::system::error_code&)>> _disconnectHandler;
    std::mutex _disconnectHandlerMtx;
    
private:
    ClientSocket(ClientSocket &) = delete;
    ClientSocket &operator=(const ClientSocket &) = delete;
    void ReadAsync();
    void WriteAsync();
    void PushWriteBuffer(std::shared_ptr<std::vector<char>> buffer);
    std::shared_ptr<std::vector<char>> GetFrontWriteBuffer();
    void PopFrontWriteBuffer();
    bool IsWriteBufferQueueEmpty();
    bool HandlePacket(int type, char *data, int length);
    void HandleError(boost::system::error_code &ec);
    std::shared_ptr<char[]> GetReceiveBuffer() { return _recvBuffer; };

public:
    explicit ClientSocket(
        boost::asio::io_context &io,
        boost::asio::ip::tcp::socket socket);
    bool Init();
    void Stop();
    bool PostWrite(std::vector<char> &data);
    //void SetMessageDeserializer(std::function<void(char *, int)> dispatcher);
    void SetPacketHandler(int type, std::function<void(char *, int)> handler);
    void SetDisconnectHandler(int type, std::function<void(boost::system::error_code&)> handler);
    void RemovePacketHandler(int type);
    void RemoveDisconnectHandler(int type);
    void ClearPacketHandler();
    void ClearDisconnectHandler();
    void SetNickname(std::string nickname);
    uint64_t GetToken() { return _token; }
    std::string GetNickname() { return _nickname; }

    virtual ~ClientSocket();
};