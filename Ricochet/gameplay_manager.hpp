#pragma once
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>
#include <map>
#include <cstdint>

class Aircraft;
class TextNode;

class GameplayManager
{
public:
	GameplayManager();

	// Player registration/unregistration
	void RegisterPlayer(uint8_t playerID, TextNode* kill_display);
	void UnregisterPlayer(uint8_t playerID);

	// Kill tracking
	void IncrementPlayerKills(uint8_t playerID);
	int GetPlayerKills(uint8_t playerID) const;
	const std::map<uint8_t, int>& GetAllPlayerKills() const;

	// Last damager tracking (for kill attribution)
	void SetPlayerLastDamager(uint8_t targetID, uint8_t damagerID);

	// Called when a player dies - awards kill to last damager
	void OnPlayerDeath(uint8_t playerID);

	// Player state tracking
	void Update();

private:
	void UpdateKillDisplay(uint8_t playerID, int kill_count);

	// Multi-player kill tracking - Maps playerID to data
	std::map<uint8_t, int> m_player_kills;
	std::map<uint8_t, TextNode*> m_player_kill_displays;
	std::map<uint8_t, bool> m_player_was_alive;
	std::map<uint8_t, uint8_t> m_last_damager;
	std::map<uint8_t, sf::Color> m_player_colors;

};
