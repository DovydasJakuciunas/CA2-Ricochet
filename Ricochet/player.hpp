#pragma once
#include "command_queue.hpp"
#include <SFML/Window/Event.hpp>
#include "action.hpp"
#include <map>
#include "command.hpp"
#include "mission_status.hpp"
#include "constants.hpp"
#include "utility.hpp"
#include <cmath>

class Player
{
public:
	Player();
	void HandleEvent(const sf::Event& event, CommandQueue& command_queue, PlayerID player_id = PlayerID::kPlayer1);
	void HandleRealTimeInput(CommandQueue& command_queue, PlayerID player_id = PlayerID::kPlayer1);

	void AssignKey(Action action, sf::Keyboard::Scancode key, PlayerID player_id = PlayerID::kPlayer1);
	sf::Keyboard::Scancode GetAssignedKey(Action action, PlayerID player_id = PlayerID::kPlayer1) const;
	void SetMissionStatus(MissionStatus status, PlayerID player_id = PlayerID::kPlayer1);
	MissionStatus GetMissionStatus(PlayerID player_id = PlayerID::kPlayer1) const;

	// PvP score tracking
	void SetPlayer1Kills(int kills);
	void SetPlayer2Kills(int kills);
	int GetPlayer1Kills() const;
	int GetPlayer2Kills() const;

private:
	void InitialiseActions();
	static bool IsRealTimeAction(Action action);

private:
	std::map<sf::Keyboard::Scancode, Action> m_key_binding_p1;
	std::map<sf::Keyboard::Scancode, Action> m_key_binding_p2;
	std::map<Action, Command> m_action_binding;
	MissionStatus m_current_mission_status_p1;
	MissionStatus m_current_mission_status_p2;
	bool m_was_forward_pressed_p1;
	bool m_was_forward_pressed_p2;

	// PvP score tracking
	int m_player1_kills;
	int m_player2_kills;
};

