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

GameplayCoordinator::GameplayCoordinator(Aircraft* player1, Aircraft* player2, SceneNode& scene_graph,
	SceneNode* upper_air_layer, const sf::FloatRect& world_bounds, const sf::View& camera,
	CommandQueue& command_queue, TextureHolder& textures, SoundPlayer& sounds)
	: m_player1(player1)
	, m_player2(player2)
	, m_scene_graph(scene_graph)
	, m_upper_air_layer(upper_air_layer)
	, m_world_bounds(world_bounds)
	, m_camera(camera)
	, m_command_queue(command_queue)
	, m_textures(textures)
	, m_sounds(sounds)
	, m_tracked_opponent(nullptr)
	, m_pickup_spawn_timer(sf::seconds(0.f))
{
	// Initialize subsystems
	m_collision_handler = std::make_unique<CollisionHandler>(m_player1, m_player2,
		m_scene_graph, m_command_queue, m_sounds);

	// Note: GameplayManager will be initialized by World with kill display UI references
	// m_gameplay_manager = std::make_unique<GameplayManager>(m_player1, m_player2, nullptr, nullptr);

	m_physics_simulator = std::make_unique<PhysicsSimulator>(m_world_bounds, m_camera);
}

void GameplayCoordinator::Update(sf::Time dt)
{

	// Use collision handler to process all collisions
	if (m_collision_handler)
	{
		m_collision_handler->HandleCollisions();
	}

	// Detect kills and handle respawns using gameplay manager
	if (m_gameplay_manager)
	{
		m_gameplay_manager->Update(m_player1, m_player2);

		// Check if players died and respawn them
		bool player1_alive = m_player1 && !m_player1->IsMarkedForRemoval();
		bool player2_alive = m_player2 && !m_player2->IsMarkedForRemoval();

		if (!player1_alive && m_player1)
		{
			m_player1->Respawn();
			m_player1->setPosition(m_world_bounds.position + sf::Vector2f(
				m_world_bounds.size.x / 4.f, m_world_bounds.size.y / 2.f));
		}

		if (!player2_alive && m_player2)
		{
			m_player2->Respawn();
			m_player2->setPosition(m_world_bounds.position + sf::Vector2f(
				3.f * m_world_bounds.size.x / 4.f, m_world_bounds.size.y / 2.f));
		}
	}


	// Use physics simulator for physics updates
	if (m_physics_simulator)
	{
		m_physics_simulator->BounceProjectiles(m_command_queue);
		m_physics_simulator->HandlePlayerBoundaryCollision(m_player1, m_player2);
	}
}

int GameplayCoordinator::GetPlayer1Kills() const
{
	return m_gameplay_manager ? m_gameplay_manager->GetPlayer1Kills() : 0;
}

int GameplayCoordinator::GetPlayer2Kills() const
{
	return m_gameplay_manager ? m_gameplay_manager->GetPlayer2Kills() : 0;
}

void GameplayCoordinator::IncrementPlayer1Kills()
{
	if (m_gameplay_manager)
	{
		m_gameplay_manager->IncrementPlayer1Kills();
	}
}

void GameplayCoordinator::IncrementPlayer2Kills()
{
	if (m_gameplay_manager)
	{
		m_gameplay_manager->IncrementPlayer2Kills();
	}
}

void GameplayCoordinator::RespawnDeadPlayers(const sf::Vector2f& spawn_pos_p1, const sf::Vector2f& spawn_pos_p2)
{
	if (m_player1 && m_player1->IsMarkedForRemoval())
	{
		m_player1->Respawn();
		m_player1->setPosition(spawn_pos_p1);
	}

	if (m_player2 && m_player2->IsMarkedForRemoval())
	{
		m_player2->Respawn();
		m_player2->setPosition(spawn_pos_p2);
	}
}
