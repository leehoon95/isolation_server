#include <boost/asio.hpp>
#include <set>
#include <queue>
#include "TestAsyncServer.h"
#include "protoc/proto_message.pb.h"
#include "redisService.h"

using namespace boost;
using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
	tcp::socket _socket;
	asio::strand<asio::io_context::executor_type> _strand;
	std::queue<std::string> _sendStringQueue;

	enum class RecvBufSize : int
	{
		recv_max_length = 1024 * 1024 * 2,
		send_max_length = 1024
	};
	char _bufferReceived[static_cast<int>(RecvBufSize::recv_max_length)];
	char _bufferSend[static_cast<int>(RecvBufSize::send_max_length)];
	char _bufferNotify[static_cast<int>(RecvBufSize::send_max_length)];
	int _remainedLengthToReceive = 0;
	int _allDataSize = 0;

public:
	explicit Session(asio::ip::tcp::socket socket,
					 asio::strand<asio::io_context::executor_type> strand)
		: _socket(std::move(socket)), _strand(strand)
	{
	}

	void Start()
	{
		do_read();
	}

	void Stop()
	{
		system::error_code ec;
		_socket.shutdown(tcp::socket::shutdown_both, ec);
		_socket.close(ec);
	}

	void AsyncWriteNotify(std::string &message)
	{
		boost::asio::post(_strand,
						  [self = shared_from_this(), msg = message]()
						  {
							  memcpy(self->_bufferSend, msg.data(), msg.length());

							  self->do_write(msg.length());
						  });

		// do_write(message.length());
		//  boost::asio::async_write(
		//  	_socket,
		//  	boost::asio::buffer(_bufferNotify, message.length()),
		//  	[this](boost::system::error_code ec, std::size_t transffered)
		//  	{
		//  		if (!ec)
		//  		{
		//  			std::cout << "notified :" << transffered << std::endl;
		//  		}
		//  	});
	}

private:
	void do_read()
	{
		// 가용한 만큼의 데이터를 한 번 읽음
		_socket.async_read_some(
			boost::asio::buffer(_bufferReceived, static_cast<int>(RecvBufSize::recv_max_length)),
			// boost::asio::bind_executor(_strand,
			[this](boost::system::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					if (length == 0)
					{
						std::cout << "client is closed" << std::endl;

						Stop();

						return;
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
							std::string response{std::format("thanks! received all data {} byte\n", _allDataSize)};
							memcpy(_bufferSend, response.c_str(), response.length());

							do_write(response.length());

							_allDataSize = 0;
						}
					}
					else if (memcmp(_bufferReceived, "prot", 4) == 0)
					{
						int serializedLength = *(int *)(_bufferReceived + 4);
						std::cout << "prot len: " << serializedLength << std::endl;

						char *serialized = _bufferReceived + 8;

						// protobuf 프로토콜콜
						//MessageType msg;
						// msg.ParseFromArray(static_cast<void *>(serialized), serializedLength);

						// MessageType type = msg.type();
						// std::cout << "type: " << static_cast<int>(type) << std::endl;

						// std::string serializedString;

						// for (const auto &character : msg.characters())
						// {
						// 	serializedString +=
						// 		std::format("id:{},x:{},y:{},z:{},***",
						// 					character.id(),
						// 					character.x(),
						// 					character.y(),
						// 					character.z());
						// }

						//std::cout << "prot: " << serializedString << std::endl;

						//memcpy(_bufferSend, serializedString.c_str(), serializedString.length());

						//do_write(serializedString.length());
					}
					else if (memcmp(_bufferReceived, "many", 4) == 0)
					{
						// 평문
						// auto data = std::string{_bufferReceived, length};

						// std::cout << std::format("ServerRecv:{}", data) << std::endl;

						// memcpy(_bufferSend, data.c_str(), data.length());

						// do_write(data.length());

						int len = *(int *)(_bufferReceived + 4);

						if (length < len)
						{
							_remainedLengthToReceive = len - length;
							_allDataSize = len;
						}

						std::cout << std::format("Received from client {} / {} ({} %)\n", length, len, (double)length / len);

						std::string response{std::format("thanks! {} byte\n", length)};
						memcpy(_bufferSend, response.c_str(), response.length());
					}
					else
					{
						std::cout << std::format("Unspecified packet ({} byte)\n", length);

						std::string response{std::format("Unspecified packet\n", length)};
						memcpy(_bufferSend, response.c_str(), response.length());

						do_write(response.length());
					}
				}
				else if (ec == asio::error::eof)
				{
					std::cout << "client is closed" << std::endl;
					Stop();

					return;
				}

				do_read();
			}
			//)
		);
	}

	void do_write(std::size_t length)
	{
		auto self = shared_from_this();
		//_socket.async_send -> 성능을 최젃화하고 전송을 세밀하게 제어하고 싶다

		// 내부에서 tcp::socket::async_send를 여러번 자동으로 호출해줌
		// 오버헤드는 있으나 반복 호출 불필요하고 전체 데이터 전송 보장함 -> 정확하고 안전한 전체 전송

		boost::asio::async_write(
			_socket,
			boost::asio::buffer(_bufferSend, length),
			asio::bind_executor(_strand,
								[this, self](boost::system::error_code ec, std::size_t transffered)
								{
									if (!ec)
									{
										std::cout << "transffered :" << transffered << std::endl;
										// do_read();
									}
								}));

		// boost::asio::async_write(
		// 	_socket,
		// 	boost::asio::buffer(_bufferSend, length),
		// 	[this, self](boost::system::error_code ec, std::size_t transffered)
		// 	{
		// 		if (!ec)
		// 		{
		// 			std::cout << "transffered :" << transffered << std::endl;
		// 			// do_read();
		// 		}
		// 	});
	}
};

class Server
{
	asio::io_context &_io;
	tcp::acceptor _acceptor;
	asio::strand<asio::io_context::executor_type> _strand;

	std::set<std::shared_ptr<Session>> _sessions;
	std::atomic<bool> _isStopped;
	RS _rs;

public:
	Server(boost::asio::io_context &io_context, uint16_t port)
		: _io(io_context),
		  _strand(asio::make_strand(io_context)),
		  _acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
		  _isStopped(false),
		  _rs(io_context)
	{
		// auto le = _acceptor.local_endpoint();
		// std::cout << "accept ep:" << le.address().to_string() << ", port: " << le.port() << std::endl;
		do_accept();
	}

	void Stop()
	{
		if (_isStopped)
			return;

		_isStopped = true;
		system::error_code ec;
		_acceptor.close(ec);

		for (auto &session : _sessions)
		{
			session->Stop();
		}

		_sessions.clear();

		_io.stop();
	}

	void Notify(std::string message)
	{
		if (_isStopped)
			return;

		for (auto &session : _sessions)
		{
			session->AsyncWriteNotify(message);
		}
	}

private:
	void do_accept()
	{
		_acceptor.async_accept(
			[this](boost::system::error_code ec, asio::ip::tcp::socket socket)
			{
				if (!ec && !_isStopped)
				{
					std::cout << "accept!\n";
					auto re = socket.remote_endpoint();
					std::cout << "client: " << re << "(" << re.port() << ")" << std::endl;

					{
						asio::ip::tcp::no_delay noDelay(true);
						socket.set_option(noDelay);
					}

					auto session = std::make_shared<Session>(std::move(socket), _strand);
					_sessions.insert(session);
					session->Start();

					do_accept();
				}
			});
	}
};

void TestAsioAsync()
{
	try
	{
		boost::asio::io_context io_context;
		Server server{io_context, 51010};

		/*
		이르게 io_context가 종료되는 것을 막음.
		아무 작업이 안 남아 있어도 run()이 반환되지 않음.(강제)
		모든 작업을 완료하고 work_guard.reset()으로 일이 없다를 표시. run() 반환
		*/
		asio::executor_work_guard<asio::io_context::executor_type> work_guard = asio::make_work_guard(io_context);

		std::thread io_thread{
			[&]()
			{
				std::cout << "Echo server is running on port 51010...\n";
				io_context.run();
				std::cout << "io_context.run() is returned!!!\n";
			}};

		bool continueSend = true;

		// std::thread testAsyncSendThread{
		// 	[&]()
		// 	{
		// 		int i = 0;
		// 		while (continueSend)
		// 		{
		// 			server.Notify(std::format("|{}|-------------------------------------------------\n", i));
		// 			++i;
		// 			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		// 		}
		// 	}};

		while (true)
		{
			std::string cmd;
			std::getline(std::cin, cmd);

			std::cout << "cmd: " << cmd << std::endl;

			if (cmd.compare("stop") == 0)
			{
				server.Stop();
				continueSend = false;
				break;
			}
			else
			{
				server.Notify(cmd);
			}
		}

		work_guard.reset();

		// testAsyncSendThread.join();
		io_thread.join();
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}