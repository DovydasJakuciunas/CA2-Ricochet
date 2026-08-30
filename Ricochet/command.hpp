#pragma once
#include <functional>
#include "receiver_categories.hpp"
#include <SFML/System/Time.hpp>

class SceneNode;

// Player identification enum
enum class PlayerID
{
	kPlayer1,
	kPlayer2
};

struct Command
{
	Command();
	std::function<void(SceneNode&, sf::Time)> action;
	unsigned int category;
};

template<typename GameObject, typename Function>
std::function<void(SceneNode&, sf::Time)>
DerivedAction(Function fn)
{
	return [=](SceneNode& node, sf::Time dt)
		{
			//Check is the cast sage
			assert(dynamic_cast<GameObject*>(&node) != nullptr);
			//Downcast and invoke the function
			fn(static_cast<GameObject&>(node), dt);
		};
}

