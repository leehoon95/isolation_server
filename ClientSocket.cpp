#include "ClientSocket.h"
#include <iostream>
#include <format>
#include "isolation_pb/error_message.pb.h"
#include "util.h"
#include "tokenPool.h"

using namespace boost;

ClientSocket::ClientSocket(
    asio::io_context &io,
    asio::ip::tcp::socket socket)
    : _io(io),
      _socket(std::move(socket)),

      _strand(asio::make_strand(io))
{
    _token = TokenPool64::Instance().allocate();
}

void ClientSocket::ReadAsync()
{
    auto self = shared_from_this();

    _socket.async_read_some(
        asio::buffer(_recvBuffer.get(), static_cast<size_t>(BufferSize::RECV_BUFFER_SIZE)),
        [self](system::error_code ec, std::size_t length)
        {
            if (length == 0)
            {
                self->HandleError(ec);
            }
            else if (!ec)
            {
                auto buffer = self->GetReceiveBuffer();

                if (length == 0)
                {
                    self->HandlePacket(EM_Type::EM_DISCONNECTED, nullptr, 0);

                    return;
                }
                else if (memcmp(buffer.get(), "prot", 4) == 0)
                {
                    int type = *(int32_t *)(&buffer[4]);
                    int totalDataLength = *(int32_t *)(&buffer[8]);

                    std::cout << std::format("prot data length: {} / {} type: {}\n",
                                             length, totalDataLength, type);

                    char *data = &buffer[12];

                    if (totalDataLength != length)
                    {
                        std::cout << std::format("TCP serialized data is insufficient. buffer data len: {}. serialized data length: {}\n",
                                                 length, totalDataLength);
                    }
                    else
                    {
                        self->HandlePacket(type, data, length - 12);
                    }
                }

                self->ReadAsync();

                // test ----
                // else if (memcmp(_recvBuffer.get(), "many", 4) == 0)
                // {
                //     int len = *(int *)(&_recvBuffer[4]);

                //     if (length < len)
                //     {
                //         _remainedLengthToReceive = len - length;
                //         _allDataSize = len;
                //     }

                //     std::cout << std::format("Received from client {} / {} ({} %)\n", length, len, (double)length / len);

                //     std::string message{std::format("thanks! {} byte\n", length)};

                //     std::vector<char> t(message.begin(), message.end());
                //     PostWrite(t);
                // }
                // else if (_remainedLengthToReceive > 0)
                // {
                //     std::cout << std::format("Received from client {} / {} ({} %)\n", length, _allDataSize, ((float)length / _allDataSize) * 100.f);
                //     _remainedLengthToReceive -= length;

                //     if (_remainedLengthToReceive < 0)
                //     {
                //         std::cout << std::format("Ramained length to receive: {} ---!!!\n", _remainedLengthToReceive);
                //     }
                //     else if (_remainedLengthToReceive == 0)
                //     {
                //         std::string message{std::format("thanks! received all data {} byte\n", _allDataSize)};

                //         std::vector<char> t(message.begin(), message.end());
                //         PostWrite(t);

                //         _allDataSize = 0;
                //     }
                // }
                // ------
            }
            else
            {
                self->HandleError(ec);
            }
        });
}

void ClientSocket::WriteAsync()
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};

    if (_writeBufferQueue.empty())
        return;

    auto bufferShared = _writeBufferQueue.front();

    auto self = shared_from_this();

    asio::async_write(
        _socket,
        asio::buffer(*bufferShared),
        asio::bind_executor(_strand,
                            [self](system::error_code ec, std::size_t transferred)
                            {
                                if (!ec)
                                {
                                    self->PopFrontWriteBuffer();

                                    if (!self->IsWriteBufferQueueEmpty())
                                        self->WriteAsync();

                                    std::cout << "transferred: " << transferred << std::endl;
                                }
                                else
                                {
                                    std::cout << "ClientSocket.Write() error :" << ec.message() << std::endl;
                                }
                            }));
}

void ClientSocket::PushWriteBuffer(std::shared_ptr<std::vector<char>> buffer)
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};
    _writeBufferQueue.push_back(buffer);
}
std::shared_ptr<std::vector<char>> ClientSocket::GetFrontWriteBuffer()
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};

    if (_writeBufferQueue.empty())
        return std::make_shared<std::vector<char>>();

    return _writeBufferQueue.front();
}

void ClientSocket::PopFrontWriteBuffer()
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};
    if (_writeBufferQueue.empty())
        return;

    _writeBufferQueue.pop_front();
}

bool ClientSocket::IsWriteBufferQueueEmpty()
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};

    return _writeBufferQueue.empty();
}

bool ClientSocket::HandlePacket(int type, char *data, int length)
{
    std::function<void(char *, int)> handler;

    {
        std::scoped_lock<std::mutex> sl{_packetHandlerMtx};
        if (_packetHandler.find(type) == _packetHandler.end())
            return false;
        else
            handler = _packetHandler[type];
    }

    if (type == EM_Type::EM_DISCONNECTED)
    {
        auto re = _socket.remote_endpoint();
        std::cerr << std::format(
            "Client socket is closed. ({}:{})\n",
            re.address().to_string(),
            re.port());

        return true;
    }

    handler(data, length);

    return true;
}

void ClientSocket::HandleError(boost::system::error_code &ec)
{
    system::error_code ec2;
    auto re = _socket.remote_endpoint(ec2);
    if (ec2)
    {
        std::cerr << std::format("ClientSocket::HandleError. remote_endpoint. {}\n", ec2.message());
    }

    std::cout << std::format(
        "ClientSocket::HandleError. {} ({}:{})\nClient socket is closed.\n",
        ec.message(),
        re.address().to_string(),
        re.port());

    std::scoped_lock<std::mutex> sl{_disconnectHandlerMtx};

    for (auto &var : _disconnectHandler)
    {
        var.second(ec);
    }
}

bool ClientSocket::Init()
{
    _recvBuffer = std::shared_ptr<char[]>(
        new char[static_cast<size_t>(BufferSize::RECV_BUFFER_SIZE)]);

    ASSERT(_recvBuffer != nullptr, "ClientSocket. Failed to allocate receive buffer");

    ReadAsync();

    return true;
}

void ClientSocket::Stop()
{
    std::cout << std::format("STOP client {} / {}\n", _token, _nickname);
    system::error_code ec;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
    {
        std::cout << std::format("ClientSocket::Stop: socket.shutdown. {}\n", ec.message());
    }

    _socket.close(ec);
    if (ec)
    {
        std::cout << std::format("ClientSocket::Stop: socket.close. {}\n", ec.message());
    }
}

bool ClientSocket::PostWrite(std::vector<char> &data)
{
    auto buffer = std::make_shared<std::vector<char>>(std::move(data));

    auto self = shared_from_this();
    PushWriteBuffer(buffer);
    WriteAsync();
    // strand로 직렬화된 작업 안에서 strand를 사용하는 함수를 호출하지 마라
    // asio::post(_strand, [self, buffer]()
    // ...
    //         self->WriteAsync();
    // ...

    return true;
}

void ClientSocket::SetPacketHandler(int type, std::function<void(char *, int)> handler)
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    _packetHandler[type] = handler;
}

void ClientSocket::SetDisconnectHandler(int type, std::function<void(boost::system::error_code &)> handler)
{
    std::scoped_lock<std::mutex> sl{_disconnectHandlerMtx};

    _disconnectHandler[type] = handler;
}

void ClientSocket::RemovePacketHandler(int type)
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    _packetHandler.erase(type);
}

void ClientSocket::RemoveDisconnectHandler(int type)
{
    std::scoped_lock<std::mutex> sl{_disconnectHandlerMtx};

    _disconnectHandler.erase(type);
}

void ClientSocket::ClearPacketHandler()
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    _packetHandler.clear();
}

void ClientSocket::ClearDisconnectHandler()
{
    std::scoped_lock<std::mutex> sl{_disconnectHandlerMtx};

    _disconnectHandler.clear();
}

void ClientSocket::SetNickname(std::string nickname)
{
    _nickname = std::move(nickname);
}

ClientSocket::~ClientSocket()
{
    std::cout << std::format("CS {} is destroyed.\n", _nickname);
    TokenPool64::Instance().release(_token);
}