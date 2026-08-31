#include "game_state.hpp"
#include "mission_status.hpp"
#include "statestack.hpp"

GameState::GameState(StateStack& stack, Context context) : State(stack, context), m_world(*context.window, *context.fonts, *context.sound), m_player(*context.player)
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

	CommandQueue& commands = m_world.GetCommandQueue();
	m_player.HandleRealTimeInput(commands);
	return true;
}

bool GameState::HandleEvent(const sf::Event& event)
{
	CommandQueue& commands = m_world.GetCommandQueue();
	m_player.HandleEvent(event, commands);

	//Escape should bring up the pause menu
	const auto* keypress = event.getIf<sf::Event::KeyPressed>();
	if (keypress && keypress->scancode == sf::Keyboard::Scancode::Escape)
	{
		RequestStackPush(StateID::kPause);
	}
	return true;
}


