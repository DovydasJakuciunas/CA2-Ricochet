#include "gameplay_coordinator.hpp"
#include "aircraft.hpp"
#include "scene_node.hpp"
#include "collision_handler.hpp"
#include "gameplay_manager.hpp"
#include "physics_simulator.hpp"
#include "resource_identifiers.hpp"
#include "sound_player.hpp"
#include "receiver_categories.hpp"
#include "utility.hpp"
#include "pickup.hpp"
#include "scene_layers.hpp"
#include <random>
#include <iostream>

// Constructor that binds references to live player list
GameplayCoordinator::GameplayCoordinator(std::vector<Aircraft*>& players, SceneNode& scene_graph,
	SceneNode* upper_air_layer, const sf::FloatRect& world_bounds, const sf::View& camera,
	CommandQueue& command_queue, TextureHolder& textures, SoundPlayer& sounds)
	: m_players(players)
	, m_scene_graph(scene_graph)
	, m_upper_air_layer(upper_air_layer)
	, m_world_bounds(world_bounds)
	, m_camera(camera)
	, m_command_queue(command_queue)
	, m_textures(textures)
	, m_sounds(sounds)
	, m_pickup_spawn_timer(sf::seconds(0.f))
{
	// Initialize subsystems
	m_collision_handler = std::make_unique<CollisionHandler>(m_players,
		m_scene_graph, m_command_queue, m_sounds);

	// Note: GameplayManager will be initialized by World with kill display UI references
	// m_gameplay_manager = std::make_unique<GameplayManager>(m_players);

	m_physics_simulator = std::make_unique<PhysicsSimulator>(m_world_bounds, m_camera);
}

void GameplayCoordinator::Update(sf::Time dt)
{

	// Use collision handler to process all collisions
	if (m_collision_handler)
	{
		m_collision_handler->HandleCollisions();
	}

	// Detect kills and handle gameplay state using gameplay manager
	if (m_gameplay_manager)
	{
		m_gameplay_manager->Update();
	}

	// Use physics simulator for physics updates
	if (m_physics_simulator)
	{
		m_physics_simulator->BounceProjectiles(m_command_queue);
		m_physics_simulator->HandlePlayerBoundaryCollision(m_players);
	}

}

void GameplayCoordinator::RespawnDeadPlayers(const std::vector<sf::Vector2f>& spawn_positions)
{
	for (size_t i = 0; i < m_players.size() && i < spawn_positions.size(); ++i)
	{
		Aircraft* player = m_players[i];
		if (player && player->IsMarkedForRemoval())
		{
			player->Respawn();
			player->setPosition(spawn_positions[i]);
		}
	}
}
