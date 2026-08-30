#include "game.hpp"
#include "utility.hpp"
#include "constants.hpp"
#include "scene_node.hpp"

Game::Game()
	: m_window(sf::VideoMode({ 1024, 1000 }), "SFML Refactor"), m_world(m_window)
{
}

void Game::Run()
{
	sf::Clock clock;
	sf::Time time_since_last_update = sf::Time::Zero;
	while (m_window.isOpen())
	{
		time_since_last_update += clock.restart();
		while (time_since_last_update.asSeconds() > kTimePerFrame)
		{
			time_since_last_update -= sf::seconds(kTimePerFrame);
			ProcessInput();
			Update(sf::seconds(kTimePerFrame));
		}
		Render();
	}
}

void Game::ProcessInput()
{
	CommandQueue& commands = m_world.GetCommandQueue();

	while (const std::optional event = m_window.pollEvent())
	{
		m_player.HandleEvent(*event, commands);

		if (event->is<sf::Event::Closed>())
		{
			m_window.close();
			break;
		}
	}
	m_player.HandleRealTimeInput(commands);
}


void Game::Update(sf::Time delta_time)
{
	m_world.Update(delta_time);

}

void Game::Render()
{
	m_window.clear(sf::Color(0, 0, 0));
	m_world.Draw();
	m_window.display();
}
