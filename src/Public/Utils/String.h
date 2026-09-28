#pragma once

#include <charconv>
#include <stdexcept>
#include <string>

namespace fc::Utils::String
{

inline constexpr uint32_t FNV1A_OFFSET_BASIS = 2166136261u;
inline constexpr uint32_t FNV1A_PRIME = 16777619u;

/// @brief Accumulates an FNV-1a hash over a sequence of strings
class HashAccumulator
{
public:
    void Add(const std::string& str)
    {
        Add(str.c_str(), str.size());
    }

    void Add(const char* data, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            mHash ^= static_cast<uint32_t>(static_cast<unsigned char>(data[i]));
            mHash *= FNV1A_PRIME;
        }
    }

    uint32_t Value() const
    {
        return mHash;
    }

private:
    uint32_t mHash = FNV1A_OFFSET_BASIS;
};

/// @brief FNV-1a hash function
inline uint32_t HashString(const std::string& str)
{
    HashAccumulator accumulator;
    accumulator.Add(str);
    return accumulator.Value();
}

inline unsigned char ParseHexByte(const std::string& str, const size_t pos)
{
    unsigned int v = 0;
    auto [ptr, ec] = std::from_chars(str.data() + pos, str.data() + pos + 2, v, 16);
    if (ec != std::errc() || ptr != str.data() + pos + 2)
        throw std::invalid_argument("Invalid hex color: " + str);
    return static_cast<unsigned char>(v);
}

} // namespace fc::Utils::String