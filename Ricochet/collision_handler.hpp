#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "scene_node.hpp"
#include "receiver_categories.hpp"
#include <set>
#include <functional>
#include <cstdint>

class Aircraft;
class CommandQueue;
class SoundPlayer;

class CollisionHandler
{
public:
	using PickupCollectedCallback = std::function<void(uint32_t)>;  // Callback with pickup ID

	CollisionHandler(std::vector<Aircraft*>& players, SceneNode& scene_graph, 
					CommandQueue& command_queue, SoundPlayer& sounds, bool is_host = true);

	void HandleCollisions();
	void SetIsHost(bool is_host);
	void SetPickupCollectedCallback(PickupCollectedCallback callback);

private:
	bool MatchesCategories(SceneNode::Pair& colliders, ReceiverCategories type1, ReceiverCategories type2) const;

	// References to game objects
	std::vector<Aircraft*>& m_players;
	SceneNode& m_scene_graph;
	CommandQueue& m_command_queue;
	SoundPlayer& m_sounds;
	bool m_is_host;
	PickupCollectedCallback m_pickup_collected_callback;
};
