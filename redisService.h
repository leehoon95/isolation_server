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
    static RS& Instance()
    {
        static RS rs;

        return rs;
    }

    bool Set(std::string_view key, std::string_view value);
    std::optional<std::string> Get(std::string_view key);
    bool Exists(std::string_view key);
    bool HashExists(std::string_view key, std::string_view field);
    bool HashSet(std::string_view key, std::string_view field, std::string_view value);
    void HashSet(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> list);
    std::optional<std::string> HashGet(std::string_view key, std::string_view field);
    bool Expire(std::string_view key, int duration);
};