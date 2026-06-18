#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <memory>
#include <format>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "Acceptor.h"
#include "Server.h"

#include "redisService.h"

using namespace boost;
using boost::asio::ip::tcp;

void TestAsioSync();

int main()
{
	try
	{
		std::cout << std::unitbuf;

		auto& rs = RS::Instance();
		auto scanResult{rs.Scan("client:*")};
		auto scanResult2{rs.Scan("logined:*")};
		scanResult.insert(
			scanResult.end(),
			std::make_move_iterator(scanResult2.begin()),
			std::make_move_iterator(scanResult2.end())
		);

		for (auto& key : scanResult)
		{
			rs.Del(key);
		}

		boost::asio::io_context io_context;
		auto server = std::make_shared<Server>(io_context);
		asio::executor_work_guard<asio::io_context::executor_type> work_guard = asio::make_work_guard(io_context);
		unsigned int clientIndex = 0;
		Acceptor acceptor(io_context, 51010);
		std::vector<std::thread> ioThreads;
		auto concurrency = std::thread::hardware_concurrency() / 2;

		for (int i = 0; i < concurrency; ++i)
		{
			ioThreads.emplace_back([&, i]()
								   {
				std::cout << std::format("io_context {} run ...\n", i);
				io_context.run();
				std::cout << std::format("io_context {} is returned\n", i); });
		}

		std::weak_ptr<Server> wserver{server};
		acceptor.Accept(
			[wserver, &io_context](asio::ip::tcp::socket socket)
			{
				if (auto server = wserver.lock())
				{
					server->AddClient(
						std::make_shared<ClientSocket>(io_context, std::move(socket)));
				}
			});

		//server->CacheLobbyList();

		while (true)
		{
			std::string cmd;
			std::getline(std::cin, cmd);

			std::cout << "cmd: " << cmd << std::endl;

			if (cmd.compare("stop") == 0)
			{
				acceptor.Stop();

				break;
			}
			else if (cmd.compare("status") == 0)
			{
				server->PrintStatus();
			}
		}

		work_guard.reset();
		io_context.stop();

		for (auto &th : ioThreads)
		{
			th.join();
		}
		// ioThread.join();

		std::cout << "All io thread is joined\n";
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	std::cout << "Server closed\n";

	auto &rs = RS::Instance();
	rs.FlushAll();

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