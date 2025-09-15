#include <sw/redis++/redis++.h>
#include <boost/asio.hpp>
#include <string>
#include <functional>
#include <queue>
#include <future>
#include <condition_variable>

class RS
{
    sw::redis::Redis _redis;

private:
    RS();
    RS(RS &rs) = delete;
    RS &operator=(const RS &) = delete;

public:
    static RS &Instance()
    {
        static RS rs;

        return rs;
    }

    // Common
    bool Set(std::string_view key, std::string_view value);
    std::optional<std::string> Get(std::string_view key);
    bool Exists(std::string_view key);
    bool Del(std::string_view key);
    
    // Hash
    bool HashFieldExists(std::string_view key, std::string_view field);
    bool HashSet(std::string_view key, std::string_view field, std::string_view value);
    void HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list);
    std::optional<std::string> HashGet(std::string_view key, std::string_view field);
    
    // Sets
    bool SetAdd(std::string_view key, std::string_view member);
    bool SetRemove(std::string_view key, std::string_view memver);
    bool SetMemberExists(std::string_view key, std::string_view member);
    std::vector<std::string> SetMembers(std::string_view key);
    unsigned int SetCardinality(std::string_view key);
    
    bool Expire(std::string_view key, int minutes);
};