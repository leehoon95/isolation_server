#pragma once
#include <sw/redis++/redis++.h>
#include <boost/asio.hpp>
#include <string>
#include <functional>
#include <queue>
#include <future>
#include <condition_variable>
#include "redisInterface.h"

class RedisService : public IRedisService
{
    std::unique_ptr<sw::redis::Redis> _redis;

public:
    RedisService();
    RedisService(RedisService &rs) = delete;
    RedisService &operator=(const RedisService &) = delete;

    void Ping() override;
    bool Set(std::string_view key, std::string_view value) override;
    std::optional<std::string> Get(std::string_view key) override;
    bool Exists(std::string_view key) override;
    bool Del(std::string_view key) override;
    bool Persist(std::string_view key) override;
    void FlushAll() override;
    std::vector<std::string> Scan(std::string_view pattern) override;
    
    // Hash
    bool HashFieldExists(std::string_view key, std::string_view field) override;
    bool HashSet(std::string_view key, std::string_view field, std::string_view value) override;
    void HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list) override;
    long long HashLen(std::string_view key);
    std::optional<std::string> HashGet(std::string_view key, std::string_view field) override;
    bool HashDel(std::string_view key, std::string_view field) override;
    
    // Sets
    bool SetAdd(std::string_view key, std::string_view member) override;
    bool SetRemove(std::string_view key, std::string_view member) override;
    bool SetMemberExists(std::string_view key, std::string_view member) override;
    std::vector<std::string> SetMembers(std::string_view key) override;
    unsigned int SetCardinality(std::string_view key) override;
    
    bool Expire(std::string_view key, int minutes) override;

    virtual ~RedisService() override = default;
};