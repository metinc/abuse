#include "coop_state.h"
#include "player_identity.h"

#include <limits>
#include <nlohmann/json.hpp>

std::string coop_state::encode() const
{
    nlohmann::json data = {{"level", level_path}, {"difficulty", difficulty}, {"players", nlohmann::json::object()}};
    for (const auto &[id, inventory] : players)
        data["players"][id] = {{"weapons", inventory.weapons}, {"current_weapon", inventory.current_weapon}};
    return data.dump();
}

bool coop_state::decode(std::string_view data, int weapon_count, coop_state &result)
{
    try
    {
        if (data.size() > 1024 * 1024 || weapon_count < 1 || weapon_count > 256)
            return false;
        const auto json = nlohmann::json::parse(data);
        coop_state decoded;
        decoded.level_path = json.at("level").get<std::string>();
        decoded.difficulty = json.at("difficulty").get<std::string>();
        if (decoded.level_path.empty() || decoded.level_path.size() > 240 ||
            decoded.level_path.find('\0') != std::string::npos ||
            (decoded.difficulty != "easy" && decoded.difficulty != "medium" && decoded.difficulty != "hard" &&
             decoded.difficulty != "extreme"))
            return false;
        const auto &players = json.at("players");
        if (!players.is_object() || players.empty() || players.size() > 1024)
            return false;
        for (const auto &[id, entry] : players.items())
        {
            if (!valid_player_id(id))
                return false;
            const auto &weapons = entry.at("weapons");
            const auto &selected = entry.at("current_weapon");
            if (!weapons.is_array() || weapons.size() != static_cast<size_t>(weapon_count) ||
                !selected.is_number_integer() ||
                (selected.is_number_unsigned() && selected.get<uint64_t>() >= static_cast<uint64_t>(weapon_count)))
                return false;
            const int64_t current = selected.get<int64_t>();
            if (current < 0 || current >= weapon_count)
                return false;
            coop_inventory inventory;
            inventory.current_weapon = static_cast<int32_t>(current);
            for (const auto &ammo : weapons)
            {
                if (!ammo.is_number_integer() ||
                    (ammo.is_number_unsigned() && ammo.get<uint64_t>() > std::numeric_limits<int32_t>::max()))
                    return false;
                const int64_t amount = ammo.get<int64_t>();
                if (amount < -1 || amount > std::numeric_limits<int32_t>::max())
                    return false;
                inventory.weapons.push_back(static_cast<int32_t>(amount));
            }
            decoded.players.emplace(id, std::move(inventory));
        }
        result = std::move(decoded);
        return true;
    }
    catch (const nlohmann::json::exception &)
    {
        return false;
    }
}
