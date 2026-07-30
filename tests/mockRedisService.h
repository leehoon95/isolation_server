#include "../src/redisInterface.h"
#include <gmock/gmock.h>

class MockRedisService : public IRedisService
{
public:
    MOCK_METHOD(void, Ping, (), (override));

    // Key - Value
    MOCK_METHOD(bool, Set, (std::string_view key, std::string_view value), (override));
    MOCK_METHOD(std::optional<std::string>, Get, (std::string_view key), (override));
    MOCK_METHOD(bool, Exists, (std::string_view key), (override));
    MOCK_METHOD(bool, Del, (std::string_view key), (override));
    MOCK_METHOD(bool, Persist, (std::string_view key), (override));
    MOCK_METHOD(void, FlushAll, (), (override));
    MOCK_METHOD(std::vector<std::string>, Scan, (std::string_view pattern), (override));

    // Hash
    MOCK_METHOD(bool, HashFieldExists, (std::string_view key, std::string_view field), (override));
    MOCK_METHOD(bool, HashSet, (std::string_view key, std::string_view field, std::string_view value), (override));
    MOCK_METHOD(void, HashSet, (std::string_view key, (std::initializer_list<std::pair<std::string_view, std::string_view>> list)), (override));
    MOCK_METHOD(long long, HashLen, (std::string_view key), (override));
    MOCK_METHOD(std::optional<std::string>, HashGet, (std::string_view key, std::string_view field), (override));
    MOCK_METHOD(bool, HashDel, (std::string_view key, std::string_view field), (override));

    // Sets
    MOCK_METHOD(bool, SetAdd, (std::string_view key, std::string_view member), (override));
    MOCK_METHOD(bool, SetRemove, (std::string_view key, std::string_view member), (override));
    MOCK_METHOD(bool, SetMemberExists, (std::string_view key, std::string_view member), (override));
    MOCK_METHOD(std::vector<std::string>, SetMembers, (std::string_view key), (override));
    MOCK_METHOD(unsigned int, SetCardinality, (std::string_view key), (override));

    // Expire
    MOCK_METHOD(bool, Expire, (std::string_view key, int minutes), (override));

    virtual ~MockRedisService() = default;
};