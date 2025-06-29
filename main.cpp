#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <memory>
#include <format>
#include <set>
#include "TestAsyncServer.h"
#include "Server.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "Acceptor.h"

using namespace boost;
using boost::asio::ip::tcp;

void TestAsioSync();

int main()
{
	try
	{
		boost::asio::io_context io_context;
		//std::map<unsigned int, std::shared_ptr<Server>> Servers;
		auto server = std::make_shared<Server>(io_context);
		asio::executor_work_guard<asio::io_context::executor_type> work_guard = asio::make_work_guard(io_context);
		unsigned int clientIndex = 0;
		// Server server{io_context, 51010};
		Acceptor acceptor(io_context, 51010);

		std::vector<std::thread> ioThreads;

		std::thread ioThread{[&](){
			std::cout << "Echo server is running on port 51010...\n";
				io_context.run();
				std::cout << "io_context.run() is returned!!!\n";
		}};

		// for (int i = 0; i < 16; ++i) {
		// 	ioThreads.emplace_back([&]()
		// 	{
		// 		std::cout << "Echo server is running on port 51010...\n";
		// 		io_context.run();
		// 		std::cout << "io_context.run() is returned!!!\n";
		// 	});
		// }

		// std::thread io_thread{
		// 	[&]()
		// 	{
		// 		std::cout << "Echo server is running on port 51010...\n";
		// 		io_context.run();
		// 		std::cout << "io_context.run() is returned!!!\n";
		// 	}};
			
		acceptor.Accept([&](asio::ip::tcp::socket socket){
			//auto server = std::make_shared<Server>(std::move(socket));
			
			server->AddClient(
				std::make_shared<ClientSocket>(io_context, std::move(socket))
			);
		});

		while (true)
		{
			std::string cmd;
			std::getline(std::cin, cmd);

			std::cout << "cmd: " << cmd << std::endl;

			if (cmd.compare("stop") == 0)
			{
				acceptor.Stop();
				server->Stop();
				// for (auto& client : Servers){
				// 	client.second->Stop();
				// }
				break;
			}
			else if (cmd.compare("status") == 0)
			{
				server->PrintStatus();
			}
			else
			{
				//server.Notify(cmd);
			}
		}

		work_guard.reset();

		// testAsyncSendThread.join();
		//io_thread.join();

		// for (auto& th : ioThreads) {
		// 	th.join();
		// }
		ioThread.join();

		std::cout << "All io thread is joined\n";
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	// TestAsioAsync();

	return 0;
}

int TestRedis()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("socket");
		return 1;
	}

	sockaddr_in server{};
	server.sin_family = AF_INET;
	server.sin_port = htons(6379);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

	if (connect(sock, (sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect");
		return 1;
	}

	// Redis 프로토콜로 SET mykey hello
	std::string cmd = "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$5\r\nhello\r\n";
	send(sock, cmd.c_str(), cmd.size(), 0);

	char buffer[1024] = {};
	int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (len > 0)
	{
		std::cout << "Redis Response: " << std::string(buffer, len) << std::endl;
	}

	close(sock);

	return 0;
}

void TestAsioSync()
{
	try
	{
		asio::io_context ic;

		tcp::acceptor acceptor{ic, tcp::endpoint{tcp::v4(), 51010}};

		std::cout << "Echo server is running on port 12345...\n";

		while (true)
		{
			tcp::socket socket{ic};

			acceptor.accept(socket);

			std::cout << "Client connected: " << socket.remote_endpoint() << "\n";

			char data[2048];

			while (true)
			{
				system::error_code error;

				auto len = socket.read_some(asio::buffer(data), error);

				if (error == asio::error::eof)
				{
					std::cout << "Connection closed by client\n";
					socket.close();
					break;
				}
				else if (error)
				{
					throw system::system_error(error);
				}

				std::string fromClient{data, len};

				if (fromClient.compare("cmd:close"))
				{
					std::string msg{"server will disconnect you."};
					asio::write(socket, asio::buffer(msg.c_str(), msg.length()));
					socket.close();
					break;
				}

				std::string str{std::format("server:your data:{}***", std::string{data, len})};

				asio::write(socket, asio::buffer(str.c_str(), str.length()));
			}
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Server failed: " << e.what() << std::endl;
	}
}