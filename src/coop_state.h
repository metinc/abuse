#ifndef ABUSE_COOP_STATE_H
#define ABUSE_COOP_STATE_H

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct coop_inventory
{
    std::vector<int32_t> weapons;
    int32_t current_weapon = 0;
};

struct coop_state
{
    std::string level_path;
    std::string difficulty;
    std::map<std::string, coop_inventory> players;

    std::string encode() const;
    static bool decode(std::string_view data, int weapon_count, coop_state &result);
};

std::string coop_save_path();
bool coop_save_available();

#endif
