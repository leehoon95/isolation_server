#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../src/Acceptor.h"
#include "../src/Server.h"
#include "mockRedisService.h"
#include "mockClientSocket.h"
#include "authentication_message.pb.h"
#include "error_message.pb.h"

using namespace testing;
using namespace boost;

TEST(Server, AcceptClient) {
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

        std::promise<void> accepted;
        auto acceptedFuture = accepted.get_future();

        acceptor.Accept(
                [&count, &accepted](asio::ip::tcp::socket socket)
                {
                    count = 1;
                    accepted.set_value();
                });

        asio::ip::tcp::socket clinetSocket(io_context);
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"), endpoint.port());

        clinetSocket.connect(ep);
        ASSERT_EQ(acceptedFuture.wait_for(std::chrono::seconds(3)), std::future_status::ready);

        clinetSocket.shutdown(asio::ip::tcp::socket::shutdown_both);
        clinetSocket.close();
        
        work_guard.reset();
        io_context.stop();
        
        if (ioThread.joinable())
            ioThread.join();

        EXPECT_EQ(count, 1);
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        FAIL();
    }
    catch (...)
    {
        std::cerr << "Unexpected exception" << std::endl;
        FAIL();
    }
}

TEST(Server, AddClient)
{
    asio::io_context io_context;
    auto rs = std::make_shared<MockRedisService>();
    auto work_guard 
        = asio::make_work_guard(io_context);
    auto server = std::make_shared<Server>(io_context, rs);
    auto mockClient = std::make_shared<MockClientSocket>();

    EXPECT_CALL(
        *mockClient, 
        Init()).Times(1);

    EXPECT_CALL(
        *mockClient, 
        GetToken()).Times(testing::AnyNumber());

    EXPECT_CALL(
        *rs, 
        HashSet(testing::_, testing::_, testing::_)).Times(testing::AnyNumber());

    EXPECT_CALL(
        *mockClient, 
        SetPacketHandler(ProtoAuthenticationMessage::REQUEST_LOGIN, testing::_)).Times(1);
    EXPECT_CALL(
        *mockClient, 
        SetPacketHandler(ProtoAuthenticationMessage::REQUEST_REGISTER_ACCOUNT, testing::_)).Times(1);
    EXPECT_CALL(
        *mockClient, 
        SetPacketHandler(ProtoAuthenticationMessage::REQUEST_PLAYER_DATA, testing::_)).Times(1);
    EXPECT_CALL(
        *mockClient, 
        SetPacketHandler(ProtoAuthenticationMessage::REQUEST_LOGOUT, testing::_)).Times(1);

    EXPECT_CALL(
        *mockClient, 
        SetErrorHandler(EM_Type::EM_DISCONNECTED, testing::_)).Times(1);
        
    server->AddClient(mockClient);
    work_guard.reset();
    io_context.stop();
}