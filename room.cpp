#include "room.h"
#include <cassert>

static_assert(std::is_trivially_copyable<ObjectTransform>::value, "memcpy unsafe for non-trivially-copyable types");

void Room::EnterRoom(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_mtx};
    for (auto &var : _clients)
    {
        if (var->GetIndex() == client->GetIndex())
            return;
    }
    ObjectTransform a, b;
    a = b;
    _clients.push_back(client);
}

void Room::ExitRoom(std::shared_ptr<ClientSocket> client)
{
    std::scoped_lock sl{_mtx};

    bool found = false;
    auto iter = _clients.begin();

    for (; iter != _clients.end(); ++iter)
    {
        if ((*iter)->GetIndex() == client->GetIndex())
            break;
    }

    // for (auto& var : _clients)
    // {
    //     if (var->GetIndex() == client->GetIndex()) {
    //         found = true;
    //         break;
    //     }
    // }

    _clients.erase(iter);
}

void Room::ReportClientTransform(int index, ObjectTransform *ot)
{
    std::scoped_lock sl{_mtx};

    if (_transforms.find(index) != _transforms.end())
    {
        _transforms[index] = *ot;
    }
}