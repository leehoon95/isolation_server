#pragma once
#include <boost/asio.hpp>
#include <set>
#include <memory>
#include <functional>

class Acceptor
{
    boost::asio::io_context &_io;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::atomic<bool> _isStopped;

    Acceptor(Acceptor &) = delete;
    Acceptor &operator=(const Acceptor &) = delete;
    
    //void AcceptV6();

public:
    explicit Acceptor(boost::asio::io_context &io, uint16_t port);
    void Accept(std::function<void(boost::asio::ip::tcp::socket)> delegator);
    void Stop();
};