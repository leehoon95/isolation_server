#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../src/Acceptor.h"

using namespace testing;
using namespace boost;

TEST(SeverTest, Accept) {
    try {
        asio::io_context io_context;
        asio::executor_work_guard<asio::io_context::executor_type> work_guard = asio::make_work_guard(io_context);
        Acceptor acceptor(io_context, 0);
        auto endpoint = acceptor.GetLocalEndpoint();
        int count = 0;

        std::thread ioThread([&io_context](){
            try 
            {
                io_context.run();
            }
            catch (std::exception& e)
            {
                std::cerr << "Exception in io_context: " << e.what() << std::endl;
                throw;
            }
        });

        acceptor.Accept(
                [&count, &io_context](asio::ip::tcp::socket socket)
                {
                    std::cout << "Client accepted!\n";
                    count = 1;
                    io_context.stop();
                });

        asio::ip::tcp::socket clinetSocket(io_context);

        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"), endpoint.port());
        clinetSocket.async_connect(ep, [](const std::error_code& error) {
            if (!error) {
                std::cout << "Connected to local server!\n";
            } else {
                std::cout << "error: " << error.message() << "\n";
            }
        });

        clinetSocket.connect(ep);
        clinetSocket.shutdown(asio::ip::tcp::socket::shutdown_both);
        clinetSocket.close();
        
        work_guard.reset();
        
        if (ioThread.joinable())
            ioThread.join();

        EXPECT_EQ(count, 1);
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unexpected exception" << std::endl;
    }
}