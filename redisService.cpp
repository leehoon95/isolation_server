#include "redisService.h"
#include <iostream>

using namespace sw;

RS::RS(boost::asio::io_context &io)
    : _redis{"tcp://127.0.0.1:6379"},
      _io(io)
{
    try
    {
        _redis.ping();
    }
    catch (redis::Error &e)
    {
        std::cout << "Redis++: ping exception. " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Redis++: Unknown Exception!!!\n";
    }
}

bool RS::Start()
{
    std::thread([this]() {

    });

    return true;
}

bool RS::Set(std::string_view key, std::string_view value, std::function<void(std::string)> completion)
{
    auto future_task = std::async(std::launch::async, [this](){

    });
    
    return true;
}

std::string RS::Get(std::string_view key)
{
    
    return "";
}