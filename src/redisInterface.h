#pragma once
#include <string>
#include <vector>
#include <optional>

class IRedisService
{
public:
    virtual void Ping() = 0;
    virtual bool Set(std::string_view key, std::string_view value) = 0;
    virtual std::optional<std::string> Get(std::string_view key) = 0;
    virtual bool Exists(std::string_view key) = 0;
    virtual bool Del(std::string_view key) = 0;
    virtual bool Persist(std::string_view key) = 0;
    virtual void FlushAll() = 0;
    virtual std::vector<std::string> Scan(std::string_view pattern) = 0;
    
    // Hash
    virtual bool HashFieldExists(std::string_view key, std::string_view field) = 0;
    virtual bool HashSet(std::string_view key, std::string_view field, std::string_view value) = 0;
    virtual void HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list) = 0;
    virtual long long HashLen(std::string_view key) = 0;
    virtual std::optional<std::string> HashGet(std::string_view key, std::string_view field) = 0;
    virtual bool HashDel(std::string_view key, std::string_view field) = 0;
    
    // Sets
    virtual bool SetAdd(std::string_view key, std::string_view member) = 0;
    virtual bool SetRemove(std::string_view key, std::string_view member) = 0;
    virtual bool SetMemberExists(std::string_view key, std::string_view member) = 0;
    virtual std::vector<std::string> SetMembers(std::string_view key) = 0;
    virtual unsigned int SetCardinality(std::string_view key) = 0;
    
    virtual bool Expire(std::string_view key, int minutes) = 0;

    virtual ~IRedisService() = default;
};