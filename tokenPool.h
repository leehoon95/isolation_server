#include <cstdint>
#include <mutex>
#include <random>
#include <unordered_set>

class TokenPool64
{
public:
    uint64_t allocate()
    {
        for (;;)
        {
            uint64_t t = dist_(engine_);
            std::lock_guard<std::mutex> lock(m_);
            auto [_, inserted] = used_.insert(t);
            if (inserted)
                return t;
        }
    }

    void release(uint64_t t)
    {
        std::lock_guard<std::mutex> lock(m_);
        used_.erase(t);
    }

private:
    std::mt19937_64 engine_{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist_{1, UINT64_MAX};
    std::unordered_set<uint64_t> used_;
    std::mutex m_;
};