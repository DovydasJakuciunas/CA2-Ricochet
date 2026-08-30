#include "game_state.hpp"
#include "mission_status.hpp"
#include "statestack.hpp"

GameState::GameState(StateStack& stack, Context context) : State(stack, context), m_world(*context.window, *context.fonts, *context.sound), m_player(*context.player), m_game_won(false)
{
	
	GetStack()->SetWorld(&m_world);
	context.music->Play(MusicThemes::kMissionTheme);
}

void GameState::Draw()
{
	m_world.Draw();
}

bool GameState::Update(sf::Time dt)
{
	m_world.Update(dt);

	// Check for win condition: first player to reach 3 kills wins
	if (!m_game_won && m_world.GetPlayer1Kills() >= 3)
	{
		m_game_won = true;
		m_player.SetMissionStatus(MissionStatus::kMissionSuccess);  // Player 1 wins
		m_player.SetPlayer1Kills(m_world.GetPlayer1Kills());
		m_player.SetPlayer2Kills(m_world.GetPlayer2Kills());
		RequestStackPush(StateID::kGameOver);
	}
	else if (!m_game_won && m_world.GetPlayer2Kills() >= 3)
	{
		m_game_won = true;
		m_player.SetMissionStatus(MissionStatus::kMissionFailure);  // Player 2 wins
		m_player.SetPlayer1Kills(m_world.GetPlayer1Kills());
		m_player.SetPlayer2Kills(m_world.GetPlayer2Kills());
		RequestStackPush(StateID::kGameOver);
	}

	CommandQueue& commands = m_world.GetCommandQueue();
	m_player.HandleRealTimeInput(commands, PlayerID::kPlayer1);
	m_player.HandleRealTimeInput(commands, PlayerID::kPlayer2);
	return true;
}

bool GameState::HandleEvent(const sf::Event& event)
{
	CommandQueue& commands = m_world.GetCommandQueue();
	m_player.HandleEvent(event, commands, PlayerID::kPlayer1);
	m_player.HandleEvent(event, commands, PlayerID::kPlayer2);

	//Escape should bring up the pause menu
	const auto* keypress = event.getIf<sf::Event::KeyPressed>();
	if (keypress && keypress->scancode == sf::Keyboard::Scancode::Escape)
	{
		RequestStackPush(StateID::kPause);
	}
	return true;
}


