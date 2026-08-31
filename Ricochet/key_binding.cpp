#include "key_binding.hpp"
#include <string>
#include <algorithm>

KeyBinding::KeyBinding(int control_preconfiguration)
	: m_key_map()
{
	// Set initial key bindings for player 1
	m_key_map[sf::Keyboard::Scancode::A] = Action::kMoveLeft;
	m_key_map[sf::Keyboard::Scancode::D] = Action::kMoveRight;
	m_key_map[sf::Keyboard::Scancode::W] = Action::kMoveUp;
	m_key_map[sf::Keyboard::Scancode::Q] = Action::kBulletFire;
	m_key_map[sf::Keyboard::Scancode::E] = Action::kMissileFire;
}

void KeyBinding::AssignKey(Action action, sf::Keyboard::Scancode key)
{
	// Remove all keys that already map to action
	for (auto itr = m_key_map.begin(); itr != m_key_map.end(); )
	{
		if (itr->second == action)
			m_key_map.erase(itr++);
		else
			++itr;
	}

	// Insert new binding
	m_key_map[key] = action;
}

sf::Keyboard::Scancode KeyBinding::GetAssignedKey(Action action) const
{
	for (auto pair : m_key_map)
	{
		if (pair.second == action)
			return pair.first;
	}

	return sf::Keyboard::Scancode::Unknown;
}

bool KeyBinding::CheckAction(sf::Keyboard::Scancode key, Action& out) const
{
	auto found = m_key_map.find(key);
	if (found == m_key_map.end())
	{
		return false;
	}
	else
	{
		out = found->second;
		return true;
	}
}

std::vector<Action> KeyBinding::GetRealtimeActions() const
{
	// Return all realtime actions that are currently active.
	std::vector<Action> actions;

	for (auto pair : m_key_map)
	{
		// If key is pressed and an action is a realtime action, store it
		if (sf::Keyboard::isKeyPressed(pair.first) && IsRealtimeAction(pair.second))
			actions.push_back(pair.second);
	}

	return actions;
}

bool IsRealtimeAction(Action action)
{
	switch (action)
	{
	case Action::kMoveLeft:
	case Action::kMoveRight:
	case Action::kMoveUp:
	case Action::kBulletFire:
		return true;

	default:
		return false;
	}
}