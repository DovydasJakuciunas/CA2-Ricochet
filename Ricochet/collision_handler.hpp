#pragma once
#include <SFML/Graphics.hpp>
#include "scene_node.hpp"
#include "receiver_categories.hpp"
#include <set>

class Aircraft;
class CommandQueue;
class SoundPlayer;

class CollisionHandler
{
public:
	CollisionHandler(Aircraft* player1, Aircraft* player2, SceneNode& scene_graph, 
					 CommandQueue& command_queue, SoundPlayer& sounds);

	void HandleCollisions();

private:
	bool MatchesCategories(SceneNode::Pair& colliders, ReceiverCategories type1, ReceiverCategories type2) const;

	// References to game objects
	Aircraft* m_player1;
	Aircraft* m_player2;
	SceneNode& m_scene_graph;
	CommandQueue& m_command_queue;
	SoundPlayer& m_sounds;
};
