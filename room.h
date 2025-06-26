#pragma once
#include "ClientSocket.h"
#include <list>
#include <mutex>
#include <map>

struct ObjectTransform
{
    int _id = 0;
    float _x = 0.f, _y = 0.f, _z = 0.f, _r = 0.f;
};

class Room : std::enable_shared_from_this<Room>
{
    std::list<std::shared_ptr<ClientSocket>> _clients;
    std::map<int, ObjectTransform> _transforms;
    std::mutex _mtx;
    std::mutex _sync;
    const float _syncRateMs = 0.02f;

public:
    void EnterRoom(std::shared_ptr<ClientSocket> client);
    void ExitRoom(std::shared_ptr<ClientSocket> client);
    void ReportClientTransform(int index, ObjectTransform *ot);
    //void SetTransfrom(int index, )
};