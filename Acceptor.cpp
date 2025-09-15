#include "Acceptor.h"
#include <iostream>
#include <format>

using namespace boost;

Acceptor::Acceptor(asio::io_context &io, uint16_t port)
    : _io(io),
      //_acceptor(io)
      _acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
//_acceptorV6(io, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port))
{
    try
    {
        // auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port);
        // _acceptor.open(endpoint.protocol());
        // _acceptor.bind(endpoint);
        _acceptor.set_option(asio::socket_base::reuse_address(true));
        //_acceptor.set_option(asio::ip::v6_only(false));
        //asio::ip::v6_only v6Only;
        //_acceptor.get_option(v6Only);
        //std::cout <<"v6 only: " << v6Only.value() << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "Exceptiopn: " << e.what() << std::endl;
    }
}

void Acceptor::Accept(std::function<void(asio::ip::tcp::socket)> delegator)
{
    _acceptor.async_accept(
        [this, delegator](system::error_code ec, asio::ip::tcp::socket socket)
        {
            if (!ec && !_isStopped)
            {
                auto re = socket.remote_endpoint();

                std::cout << std::format("accept! {}:{}", re.address().to_string(), re.port()) << std::endl;
                {
                    asio::ip::tcp::no_delay noDelay(true);
                    socket.set_option(noDelay);
                }

                delegator(std::move(socket));

                if (!_isStopped)
                    Accept(delegator);
            }
            else
            {
                std::cout << std::format("Acceptor.Accept() Error: {}\n", ec.message());
            }
        });
}

// void Acceptor::AcceptV6()
// {
//     _acceptorV6.async_accept(
//         [this](system::error_code ec, asio::ip::tcp::socket socket)
//         {
//             if (!ec && !_isStopped)
//             {
//                 auto re = socket.remote_endpoint();

//                 std::cout << std::format("accept! v6 {}:{}", re.address().to_string(), re.port());

//                 {
//                     asio::ip::tcp::no_delay noDelay(true);
//                     socket.set_option(noDelay);
//                 }
//             }

//             if (!_isStopped)
//                 AcceptV6();
//         });
// }

void Acceptor::Stop()
{
    if (_isStopped)
        return;

    _isStopped = true;

    system::error_code ec;
    _acceptor.close(ec);
}