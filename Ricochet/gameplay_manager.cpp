#include "gameplay_manager.hpp"
#include "aircraft.hpp"
#include "text_node.hpp"
#include "constants.hpp"
#include <string>

GameplayManager::GameplayManager()
{
}

void GameplayManager::RegisterPlayer(uint8_t playerID, TextNode* kill_display)
{
	if (playerID < 15)  // Support players 0-14
	{
		m_player_kills[playerID] = 0;
		m_player_kill_displays[playerID] = kill_display;
		m_player_was_alive[playerID] = true;
		m_player_colors[playerID] = PlayerColors::GetColor(playerID);

		// Initialize display
		if (kill_display)
		{
			kill_display->SetString("Player " + std::to_string(playerID + 1) + ": 0");
			kill_display->SetColor(m_player_colors[playerID]);
		}
	}
}

void GameplayManager::UnregisterPlayer(uint8_t playerID)
{
	m_player_kills.erase(playerID);
	m_player_kill_displays.erase(playerID);
	m_player_was_alive.erase(playerID);
	m_last_damager.erase(playerID);
	m_player_colors.erase(playerID);
}

void GameplayManager::IncrementPlayerKills(uint8_t playerID)
{
	if (m_player_kills.find(playerID) != m_player_kills.end())
	{
		m_player_kills[playerID]++;
		UpdateKillDisplay(playerID, m_player_kills[playerID]);
	}
}

int GameplayManager::GetPlayerKills(uint8_t playerID) const
{
	auto it = m_player_kills.find(playerID);
	return it != m_player_kills.end() ? it->second : 0;
}

const std::map<uint8_t, int>& GameplayManager::GetAllPlayerKills() const
{
	return m_player_kills;
}

void GameplayManager::SetPlayerLastDamager(uint8_t targetID, uint8_t damagerID)
{
	m_last_damager[targetID] = damagerID;
}

void GameplayManager::OnPlayerDeath(uint8_t playerID)
{
	// Award kill to the player who dealt the last hit
	auto last_hit_iter = m_last_damager.find(playerID);
	if (last_hit_iter != m_last_damager.end())
	{
		uint8_t killer = last_hit_iter->second;
		IncrementPlayerKills(killer);
		m_last_damager.erase(playerID);
	}
}

void GameplayManager::UpdateKillDisplay(uint8_t playerID, int kill_count)
{
	auto display_iter = m_player_kill_displays.find(playerID);
	if (display_iter != m_player_kill_displays.end() && display_iter->second)
	{
		TextNode* display = display_iter->second;
		std::string player_label = "Player " + std::to_string(playerID + 1) + ": ";
		display->SetString(player_label + std::to_string(kill_count));

		// Set color to match player color
		auto color_iter = m_player_colors.find(playerID);
		if (color_iter != m_player_colors.end())
		{
			display->SetColor(color_iter->second);
		}
	}
}

void GameplayManager::Update()
{
}
