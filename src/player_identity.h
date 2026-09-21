#ifndef ABUSE_PLAYER_IDENTITY_H
#define ABUSE_PLAYER_IDENTITY_H

#include <algorithm>
#include <random>
#include <string>
#include <string_view>

constexpr std::size_t PLAYER_ID_LENGTH = 32;

inline bool valid_player_id(std::string_view id)
{
    return id.size() == PLAYER_ID_LENGTH &&
           std::all_of(id.begin(), id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

inline std::string generate_player_id()
{
    // Do not consume the game's deterministic random stream.
    std::random_device random;
    std::uniform_int_distribution<unsigned int> byte(0, 255);
    constexpr char hex[] = "0123456789abcdef";
    std::string id;
    for (std::size_t i = 0; i < PLAYER_ID_LENGTH / 2; ++i)
    {
        const unsigned int value = byte(random);
        id += hex[value >> 4];
        id += hex[value & 15];
    }
    return id;
}

#endif
