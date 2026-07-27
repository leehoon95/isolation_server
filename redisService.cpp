#include "redisService.h"
#include <iostream>
#include "Config.h"

using namespace sw;

RS::RS()
{
    try
    {
        std::cout << std::format("Connect to redis-server. {0} {1}\n",
            REDIS_SERVER_HOST,
            REDIS_SERVER_PORT);

        sw::redis::ConnectionOptions options;
        options.host = REDIS_SERVER_HOST;
        options.port = std::stoi(REDIS_SERVER_PORT);

        _redis = std::make_unique<sw::redis::Redis>(options);
    }
    catch (redis::Error &e)
    {
        std::cout << "Redis++ Exception: " << e.what() << std::endl;
        throw;
    }
    catch (std::invalid_argument &e)
    {
        std::cout << "Redis++ Exception: Invalid Argument\n";
        throw;
    }
    catch (std::out_of_range &e)
    {
        std::cout << "Redis++ Exception: Out Of Range\n";
        throw;
    }
}

void RS::Ping()
{
    _redis->ping();
}

bool RS::Set(std::string_view key, std::string_view value)
{
    if (key.empty())
        return false;

    _redis->set(key, value);

    return true;
}

std::optional<std::string> RS::Get(std::string_view key)
{
    auto value = _redis->get(key);

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
    return _redis->exists(key) > 0;
}

bool RS::Del(std::string_view key)
{
    return _redis->del(key) > 0;
}

bool RS::Persist(std::string_view key)
{
    return _redis->persist(key);
}

void RS::FlushAll()
{
    _redis->flushall();
}

std::vector<std::string> RS::Scan(std::string_view pattern)
{
    sw::redis::Cursor cursor = 0;
    std::vector<std::string> data;
    while (true)
    {
        _redis->scan(cursor, pattern, std::inserter(data, data.begin()));
        if (cursor == 0)
        {
            break;
        }
    }

    return std::move(data);
}

bool RS::HashFieldExists(std::string_view key, std::string_view field)
{
    return _redis->hexists(key, field);
}

bool RS::HashSet(std::string_view key, std::string_view field, std::string_view value)
{
    return _redis->hset(key, field, value);
}

void RS::HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list)
{
    _redis->hmset(key, list);
}

std::optional<std::string> RS::HashGet(std::string_view key, std::string_view field)
{
    auto value = _redis->hget(key, field);

    if (value)
    {
        return std::move(*value);
    }

    return std::nullopt;
}

long long RS::HashLen(std::string_view key)
{
    return _redis->hlen(key);
}

bool RS::HashDel(std::string_view key, std::string_view field)
{
    return _redis->hdel(key, field);
}

bool RS::SetAdd(std::string_view key, std::string_view member)
{
    return _redis->sadd(key, member) > 0;
}

bool RS::SetRemove(std::string_view key, std::string_view memver)
{
    return _redis->srem(key, memver) > 0;
}

bool RS::SetMemberExists(std::string_view key, std::string_view member)
{
    return _redis->sismember(key, member);
}

std::vector<std::string> RS::SetMembers(std::string_view key)
{
    std::vector<std::string> members;
    _redis->smembers(key, std::back_inserter(members));

    return std::move(members);
}

unsigned int RS::SetCardinality(std::string_view key)
{
    return static_cast<int>(_redis->scard(key));
}

bool RS::Expire(std::string_view key, int minutes)
{
    return _redis->expire(
        key,
        std::chrono::minutes(minutes));
}