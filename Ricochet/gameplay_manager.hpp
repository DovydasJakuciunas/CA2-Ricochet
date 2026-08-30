#pragma once
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>

class Aircraft;
class TextNode;

class GameplayManager
{
public:
	GameplayManager(Aircraft* player1, Aircraft* player2, TextNode* kill_display1, TextNode* kill_display2);

	// Kill tracking
	int GetPlayer1Kills() const;
	int GetPlayer2Kills() const;
	void IncrementPlayer1Kills();
	void IncrementPlayer2Kills();

	// Player state tracking
	void Update(Aircraft* player1, Aircraft* player2);

private:
	void UpdateKillDisplay(TextNode* display, int kill_count);
	// Kill counts
	int m_player1_kills;
	int m_player2_kills;

	// Player alive status tracking (for detecting transitions)
	bool m_player1_was_alive;
	bool m_player2_was_alive;

	// UI references
	TextNode* m_player1_kill_display;
	TextNode* m_player2_kill_display;

};
