#pragma once
#include <SFML/Graphics.hpp>
#include "scene_node.hpp"
#include "resource_holder.hpp"
#include "texture_id.hpp"
#include "world.hpp"
#include "player.hpp"

class Game
{
public:
	Game();
	void Run();

private:
	void ProcessEvent();
	void Update(sf::Time delta_time);
	void Render();
	void ProcessInput();
	//void HandlePlayerInput(sf::Keyboard::Scancode key, bool is_pressed);


private:
	sf::RenderWindow m_window;
	World m_world;
	Player m_player;
};

