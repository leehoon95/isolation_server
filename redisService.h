#include <sw/redis++/redis++.h>
#include <boost/asio.hpp>
#include <string>
#include <functional>
#include <queue>
#include <future>
#include <condition_variable>

class RS
{
    sw::redis::Redis _redis;
    boost::asio::io_context& _io;
    std::queue<std::future<std::string>> _q;
    
    RS(RS &rs) = delete;
    RS &operator=(const RS &) = delete;

public:
    RS(boost::asio::io_context& io);

    bool Start();
    bool Set(std::string_view key, std::string_view value, std::function<void(std::string)> completion);
    std::string Get(std::string_view key);
};