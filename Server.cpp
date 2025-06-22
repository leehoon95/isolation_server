#include "Server.h"
#include <iostream>
#include "protoc/proto_message.pb.h"
#include "util.h"

using namespace boost;

Server::Server(
    asio::io_context &io)
    : _io(io)
{
}

void Server::Stop()
{
    std::scoped_lock sl{_connMtx, _loginedMtx};

    for (auto &c : _connectedClients)
    {
        c.second->Stop();
    }

    for (auto &c : _loginedClients)
    {
        c.second->Stop();
    }

    _connectedClients.clear();
    _loginedClients.clear();
}

void Server::AddClient(std::shared_ptr<ClientSocket> client)
{
    _clientIndex++;
    _connectedClients[_clientIndex] = client;
    client->Init(_clientIndex);
    client->SetProcedure(
        REQUEST_LOGIN,
        [this, client](char *serializedData, int length)
        {
            PROTO_RequestLogin msg;
            std::cout << std::format("c use_count: {}\n", client.use_count());

            if (msg.ParseFromArray(serializedData, length))
            {
                std::string nickname{msg.nickname()};

                client->SetNickname(nickname);

                {
                    std::scoped_lock sl{_connMtx, _loginedMtx};
                    auto cc = _connectedClients.find(client->GetIndex());
                    if (cc == _connectedClients.end())
                    {
                        std::cout << std::format("Not found connected client {}:{}\n",
                                                 client->GetIndex(), nickname);

                        return;
                    }

                    _loginedClients[nickname] = cc->second;
                    _connectedClients.erase(cc);
                }

                std::string message{std::format("client nickname: {}. login allowed.", nickname)};
                std::cout << message << std::endl;
                for (auto &c : message)
                {
                    printf("0x%X(%c) ", c, c);
                }

                printf("\n");

                PROTO_LoginResult lr;

                lr.set_result(true);

                std::string lrString;
                lr.SerializeToString(&lrString);

                std::vector<char> t{'p', 'r', 'o', 't'};
                append_int_vec_char_le(t, static_cast<int>(LOGIN_RESULT));
                append_int_vec_char_le(t, static_cast<int>(lrString.length()));
                t.insert(t.end(), lrString.begin(), lrString.end());
                // t.push_back(message.begin(), message.end());
                client->PostWrite(t);
                // std::cout << std::format("t.size(): {}\n", t.size());
            }
            else
            {
                std::string message{"invalid nickname"};
                std::vector<char> t(message.begin(), message.end());
                client->PostWrite(t);
                // std::cout << std::format("t.size(): {}\n", t.size());
            }
        });

    client->OnDisconnected(
        [this](unsigned int index, std::string nickname)
        {
            std::cout << std::format("client {} / {} disconnected\n", index, nickname);
            {
                std::scoped_lock sl{_connMtx, _loginedMtx};
                _connectedClients.erase(index);
                _loginedClients.erase(nickname);
            }
        });
}

void Server::PrintStatus()
{
    std::scoped_lock sl{_connMtx, _loginedMtx};

    std::cout << "--- just connected clients ---\n";
    for (auto &pair : _connectedClients)
    {
        std::cout << std::format("{}: ---\n", pair.first);
    }
    std::cout << "------\n\n";

    std::cout << "--- logined client ---\n";
    for (auto &pair : _loginedClients)
    {
        if (pair.second.use_count() > 0)
            std::cout << std::format("{}\n", pair.first);
    }
    std::cout << "------\n\n";
}