#include "ClientSocket.h"
#include <iostream>
#include <format>
#include "protoc/proto_message.pb.h"
#include "util.h"

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

//
void ClientSocket::ReadAsync()
{
    auto self = shared_from_this();
    
    _socket.async_read_some(
        asio::buffer(_recvBuffer.get(), static_cast<size_t>(BufferSize::RECV_BUFFER_SIZE)),
        [self](boost::system::error_code ec, std::size_t length)
        {
            

            if (!ec)
            {
                auto buffer = self->GetReceiveBuffer();

                if (length == 0)
                {
                    self->HandlePacket(PROTO_MessageType::DISCONNECTED, nullptr, 0);

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
            else if (ec == asio::error::eof)
            {
                self->PrintSocketErorrEof();
                self->HandlePacket(PROTO_MessageType::DISCONNECTED, nullptr, 0);

                return;
            }
            else
            {
                std::cout << std::format("Client Socket error: {}. what: {} .\n", ec.value(), ec.what());
            }
        });
}

void ClientSocket::WriteAsync()
{
    std::scoped_lock<std::mutex> sl{_writeBufferMtx};

    if (_writeBufferQueue.empty())
        return;

    _writeInProcessing = true;

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

                                    if (self->IsWriteBufferQueueEmpty())
                                        self->WriteAsync();
                                    else
                                        self->SetWriteProcessing(false);

                                    std::cout << "transferred: " << transferred << std::endl;
                                }
                                else
                                {
                                    std::cout << "ClientSocket.Write() error :" << ec.what() << std::endl;
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

bool ClientSocket::IsWriteProcessing()
{
    return _writeInProcessing.load();
}

void ClientSocket::SetWriteProcessing(bool value)
{
    _writeInProcessing = value;
}

bool ClientSocket::HandlePacket(int type, char *data, int length)
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    if (_packetHandler.find(type) == _packetHandler.end())
        return false;

    if (type == PROTO_MessageType::DISCONNECTED)
    {
        auto re = _socket.remote_endpoint();
        std::cerr << std::format(
            "Client socket is closed. ({}:{})\n",
            re.address().to_string(),
            re.port());

        if (_onDisconnected)
            _onDisconnected(_index, _nickname);

        return true;
    }
   
    if (_packetHandler.find(type) != _packetHandler.end())
        _packetHandler[type](data, length);
    else
        return false;

    return true;
}

void ClientSocket::PrintSocketErorrEof()
{
    auto re = _socket.remote_endpoint();
    std::cout << std::format(
        "Client Socket Error. eof ({}:{})\n",
        re.address().to_string(),
        re.port());
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

    auto self = shared_from_this();

    asio::post(_strand, [self, buffer]()
               {
        self->PushWriteBuffer(buffer);

        if (!self->IsWriteProcessing())
            self->WriteAsync(); });

    return true;
}

void ClientSocket::SetPacketHandler(int type, std::function<void(char *, int)> proc)
{
    // std::cout << "SetProcedure() type: " << static_cast<int>(type) << std::endl;
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    _packetHandler[type] = proc;
}

void ClientSocket::RemoveHandler(int type)
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};

    _packetHandler.erase(type);
}

// void ClientSocket::OnDisconnected(std::function<void(int, std::string)> callback)
// {
//     _onDisconnected = callback;
// }

void ClientSocket::ClearHandler()
{
    std::scoped_lock<std::mutex> sl{_packetHandlerMtx};
    _packetHandler.clear();
}

void ClientSocket::SetNickname(std::string nickname)
{
    _nickname = std::move(nickname);
}

ClientSocket::~ClientSocket()
{
    std::cout << std::format("\n", _nickname);
}