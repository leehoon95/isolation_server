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

int main()
{
	std::cout << std::unitbuf;

	char* envGitSha = getenv("GIT_COMMIT_SHA");
	if (envGitSha == nullptr)
	{
		std::cerr << "Env var Git Commit Sha is null\n";
		return -1;
	}

	std::cout << std::format("Git Commit Sha: {0}\nBuilt At: {1} {2}\n",
		envGitSha, __DATE__, __TIME__);

	char* envPort = getenv("ISOLATION_SERVER_LISTENING_PORT");
	if (envPort == nullptr)
	{
		std::cerr << "Env var listening port is null\n";
		return -1;
	}

	int port = 0;

	try
	{
		port = std::stoi(envPort);
	}
	catch (std::invalid_argument& e)
	{
		std::cerr << std::format("std::invalid_argument::what(): {0}\n", e.what());
		return -2;
	}
	catch (std::out_of_range& e)
	{
		std::cerr << std::format("std::out_of_range::what(): {0}\n", e.what());
		return -3;
	}

	std::cout << std::format("Start Isolation Server\nListening({0})...\n\n", 
		port);

	try
	{
		auto rs = std::make_unique<RedisService>("redis", 6379);
		rs->Ping();

		auto scanResult{rs->Scan("client:*")};
		auto scanResult2{rs->Scan("logined:*")};
		scanResult.insert(
			scanResult.end(),
			std::make_move_iterator(scanResult2.begin()),
			std::make_move_iterator(scanResult2.end())
		);

		for (auto& key : scanResult)
		{
			rs->Del(key);
		}

		boost::asio::io_context io_context;
		auto server = std::make_shared<Server>(io_context, std::move(rs));
		asio::executor_work_guard<asio::io_context::executor_type> work_guard 
			= asio::make_work_guard(io_context);
		unsigned int clientIndex = 0;
		Acceptor acceptor(io_context, port);
		std::vector<std::thread> ioThreads;
		auto concurrency = std::thread::hardware_concurrency() / 2;

		if (concurrency == 0) {
			concurrency = 1;
		}

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

		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&](const boost::system::error_code& error, int signal_num) {
			if (!error) {
				std::cout << "\n[SIGNAL] Shutdown signal received\n";
				work_guard.reset();
				io_context.stop();
			}
		});


		for (auto &th : ioThreads)
		{
			if (th.joinable())
				th.join();
		}

		std::cout << "All io thread is joined\n";
	}
	catch (sw::redis::Error &e) {
		std::cout << "Redis++ Exception: " << e.what() << std::endl;
		throw;
	}
	catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}