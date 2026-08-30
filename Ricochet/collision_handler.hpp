#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "scene_node.hpp"
#include "receiver_categories.hpp"
#include <set>

class Aircraft;
class CommandQueue;
class SoundPlayer;

class CollisionHandler
{
public:
	CollisionHandler(const std::vector<Aircraft*>& players, SceneNode& scene_graph, 
					CommandQueue& command_queue, SoundPlayer& sounds);

	void HandleCollisions();

private:
	bool MatchesCategories(SceneNode::Pair& colliders, ReceiverCategories type1, ReceiverCategories type2) const;

	// References to game objects
	std::vector<Aircraft*> m_players;
	SceneNode& m_scene_graph;
	CommandQueue& m_command_queue;
	SoundPlayer& m_sounds;
};
