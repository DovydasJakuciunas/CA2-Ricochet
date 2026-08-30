#pragma once
#include "state.hpp"
#include <SFML/Graphics/Text.hpp>

class GameOverState : public State
{
public:
	GameOverState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event);

private:
	sf::Text m_game_over_text;
	sf::Text m_score_text;
	sf::Time m_elapsed_time;
	bool m_is_pvp_win;
	int m_player1_kills;
	int m_player2_kills;

};

