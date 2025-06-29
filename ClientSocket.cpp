#include "ClientSocket.h"
#include <iostream>
#include <format>
#include "protoc/proto_message.pb.h"

using namespace boost;
// using asio::ip::tcp;

ClientSocket::ClientSocket(
    asio::io_context &io,
    asio::ip::tcp::socket socket)
    : _io(io),
      _socket(std::move(socket)),
      _strand(asio::make_strand(io))
{
}

void ClientSocket::ReadAsync()
{
    // auto client = shared_from_this();

    _socket.async_read_some(
        asio::buffer(_recvBuffer.get(), static_cast<size_t>(BufferSize::RECV_BUFFER_SIZE)),
        [this](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                if (length == 0)
                {
                    auto re = _socket.remote_endpoint();
                    std::cerr << std::format(
                        "Client is closed. ({}:{})\n",
                        re.address().to_string(),
                        re.port());

                    return;
                }
                else if (memcmp(_recvBuffer.get(), "prot", 4) == 0)
                {
                    int type = *(int32_t *)(&_recvBuffer[4]);
                    int totalDataLength = *(int32_t *)(&_recvBuffer[8]);

                    std::cout << std::format("prot data length: {} / {} type: {}\n",
                                             length, totalDataLength, type);

                    char *data = &_recvBuffer[12];

                    if (totalDataLength != length)
                    {
                        std::cout << std::format("TCP serialized data is insufficient. buffer data len: {}. serialized data length: {}\n",
                             length, totalDataLength);
                    }
                    else
                        _procedure[type](data, length - 12);
                }

                // test ----
                else if (memcmp(_recvBuffer.get(), "many", 4) == 0)
                {
                    int len = *(int *)(&_recvBuffer[4]);

                    if (length < len)
                    {
                        _remainedLengthToReceive = len - length;
                        _allDataSize = len;
                    }

                    std::cout << std::format("Received from client {} / {} ({} %)\n", length, len, (double)length / len);

                    std::string message{std::format("thanks! {} byte\n", length)};

                    std::vector<char> t(message.begin(), message.end());
                    PostWrite(t);
                }
                else if (_remainedLengthToReceive > 0)
                {
                    std::cout << std::format("Received from client {} / {} ({} %)\n", length, _allDataSize, ((float)length / _allDataSize) * 100.f);
                    _remainedLengthToReceive -= length;

                    if (_remainedLengthToReceive < 0)
                    {
                        std::cout << std::format("Ramained length to receive: {} ---!!!\n", _remainedLengthToReceive);
                    }
                    else if (_remainedLengthToReceive == 0)
                    {
                        std::string message{std::format("thanks! received all data {} byte\n", _allDataSize)};

                        std::vector<char> t(message.begin(), message.end());
                        PostWrite(t);

                        _allDataSize = 0;
                    }
                }
                // ------

                ReadAsync();
            }
            else if (ec == asio::error::eof)
            {
                auto re = _socket.remote_endpoint();
                std::cout << std::format(
                    "Client is closed. eof ({}:{})\n",
                    re.address().to_string(),
                    re.port());
                _onDisconnected(_index, _nickname);

                return;
            }
        });
}

void ClientSocket::WriteAsync()
{
    if (_writeBufferQueue.empty())
        return;

    _writeInProgress = true;

    auto bufferShared = _writeBufferQueue.front();

    asio::async_write(
        _socket,
        asio::buffer(*bufferShared),
        asio::bind_executor(_strand,
                            [this](system::error_code ec, std::size_t transferred)
                            {
                                if (!ec)
                                {
                                    _writeBufferQueue.pop_front();
                                    if (!_writeBufferQueue.empty())
                                        WriteAsync();
                                    else
                                        _writeInProgress = false;
                                    std::cout << "transferred: " << transferred << std::endl;
                                }
                                else
                                {
                                    std::cout << "ClientSocket.Write() error :" << ec.what() << std::endl;
                                }
                            }));
}

bool ClientSocket::Init(int index)
{
    _recvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(BufferSize::RECV_BUFFER_SIZE)]);
    _index = index;

    ReadAsync();

    return true;
}

void ClientSocket::Stop()
{
    std::cout << std::format("STOP Client {} / {}.\n", _index, _nickname);
    system::error_code ec;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);

    if (!ec)
    {
        std::cout << std::format("STOP Client {} / {}. error: {}\n", _index, _nickname, ec.what());
    }
}

bool ClientSocket::PostWrite(std::vector<char> &data)
{
    auto buffer = std::make_shared<std::vector<char>>(std::move(data));

    asio::post(_strand, [this, buffer]()
               {
        _writeBufferQueue.push_back(buffer);

        if (!_writeInProgress)
            WriteAsync(); });

    return true;
}

void ClientSocket::SetProcedure(int type, std::function<void(char *, int)> proc)
{
    std::cout << "SetProcedure() type: " << static_cast<int>(type) << std::endl;
    std::scoped_lock<std::mutex> sl{_procMtx};

    _procedure[type] = proc;
}

void ClientSocket::OnDisconnected(std::function<void(int, std::string)> callback)
{
    _onDisconnected = callback;
}

void ClientSocket::ClearProcedure()
{
    std::scoped_lock<std::mutex> sl{_procMtx};
    _procedure.clear();
}

void ClientSocket::SetNickname(std::string nickname)
{
    _nickname = std::move(nickname);
}

ClientSocket::~ClientSocket()
{
    std::cout << std::format("\n", _nickname);
}