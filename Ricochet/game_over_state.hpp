#pragma once
#include "state.hpp"
#include <SFML/Graphics/Text.hpp>
#include <map>
#include <vector>
#include <cstdint>

class GameOverState : public State
{
public:
	GameOverState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event);

private:
	sf::Text m_game_over_text;
	std::vector<sf::Text> m_player_score_texts;	// Dynamic scoreboard for all players
	sf::Time m_elapsed_time;
	uint8_t m_winner_id;						// ID of the winning player
	std::map<uint8_t, int> m_all_player_kills;	// Cache of all player kills

};

