#include "gameplay_manager.hpp"
#include "aircraft.hpp"
#include "text_node.hpp"
#include <string>

GameplayManager::GameplayManager(Aircraft* player1, Aircraft* player2, TextNode* kill_display1, TextNode* kill_display2)
	: m_player1_kills(0)
	, m_player2_kills(0)
	, m_player1_was_alive(true)
	, m_player2_was_alive(true)
	, m_player1_kill_display(kill_display1)
	, m_player2_kill_display(kill_display2)
{
}

int GameplayManager::GetPlayer1Kills() const
{
	return m_player1_kills;
}

int GameplayManager::GetPlayer2Kills() const
{
	return m_player2_kills;
}

void GameplayManager::IncrementPlayer1Kills()
{
	m_player1_kills++;
	UpdateKillDisplay(m_player1_kill_display, m_player1_kills);
}

void GameplayManager::IncrementPlayer2Kills()
{
	m_player2_kills++;
	UpdateKillDisplay(m_player2_kill_display, m_player2_kills);
}

void GameplayManager::UpdateKillDisplay(TextNode* display, int kill_count)
{
	if (display)
	{
		// Determine which player this display belongs to
		std::string player_label = (display == m_player1_kill_display) ? "P1 Kills: " : "P2 Kills: ";
		display->SetString(player_label + std::to_string(kill_count));
	}
}

void GameplayManager::Update(Aircraft* player1, Aircraft* player2)
{
	if (!player1 || !player2)
		return;

	// Detect kills - check if players changed from alive to dead
	bool player1_alive = !player1->IsMarkedForRemoval();
	bool player2_alive = !player2->IsMarkedForRemoval();

	// Player 1 was alive but is now dead - Player 2 gets a kill
	if (m_player1_was_alive && !player1_alive)
	{
		IncrementPlayer2Kills();
	}

	// Player 2 was alive but is now dead - Player 1 gets a kill
	if (m_player2_was_alive && !player2_alive)
	{
		IncrementPlayer1Kills();
	}

	// Update alive status for next frame
	m_player1_was_alive = player1_alive;
	m_player2_was_alive = player2_alive;
}
