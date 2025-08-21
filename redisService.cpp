#include "redisService.h"
#include <iostream>

using namespace sw;

RS::RS()
    : _redis{"tcp://127.0.0.1:6379"}
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

bool RS::Set(std::string_view key, std::string_view value)
{
    if (key.empty())
        return false;

    _redis.set(key, value);

    return true;
}

std::optional<std::string> RS::Get(std::string_view key)
{
    auto value = _redis.get(key);

    if (value)
    {
        std::cout << std::format("key: {}, value: {}\n", key, *value);
        return std::move(*value);
    }

    std::cout << std::format("key {} does not exist\n", key);

    return std::nullopt;
}

bool RS::Exists(std::string_view key)
{
    return _redis.exists(key) > 0;
}

bool RS::HashExists(std::string_view key, std::string_view field)
{
    return _redis.hexists(key, field);
}

bool RS::HashSet(std::string_view key, std::string_view field, std::string_view value)
{
    return _redis.hset(key, field, value);
}

void RS::HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list)
{
    _redis.hmset(key, list);
}

std::optional<std::string> RS::HashGet(std::string_view key, std::string_view field)
{
    auto value = _redis.hget(key, field);

    if (value)
    {
        return std::move(*value);
    }

    return std::nullopt;
}

bool RS::Expire(std::string_view key, int hour)
{
    return _redis.expire(
        key,
        std::chrono::hours(hour));
}