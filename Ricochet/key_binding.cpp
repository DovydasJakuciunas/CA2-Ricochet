#include "key_binding.hpp"
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

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

void KeyBinding::SaveToFile(const std::string& filename) const
{
	std::ofstream outfile(filename);
	if (!outfile.is_open())
	{
		return;
	}

	for (const auto& pair : m_key_map)
	{
		// Save: scancode=action
		outfile << static_cast<int>(pair.first) << "=" << static_cast<int>(pair.second) << std::endl;
	}

	outfile.close();
}

void KeyBinding::LoadFromFile(const std::string& filename)
{
	std::ifstream infile(filename);
	if (!infile.is_open())
	{
		return;
	}

	std::string line;
	m_key_map.clear();  // Clear existing bindings

	while (std::getline(infile, line))
	{
		if (line.empty() || line[0] == '#')
			continue;  // Skip empty lines and comments

		size_t pos = line.find('=');
		if (pos == std::string::npos)
			continue;  // Skip invalid lines

		try
		{
			int scancode_int = std::stoi(line.substr(0, pos));
			int action_int = std::stoi(line.substr(pos + 1));

			sf::Keyboard::Scancode scancode = static_cast<sf::Keyboard::Scancode>(scancode_int);
			Action action = static_cast<Action>(action_int);

			m_key_map[scancode] = action;
		}
		catch (const std::exception& e)
		{
			std::cerr << "[KeyBinding] Error parsing line: " << line << " - " << e.what() << std::endl;
		}
	}

	infile.close();
}