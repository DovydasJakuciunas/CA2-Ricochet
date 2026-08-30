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

	SpawnRandomPickups();

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

Aircraft* GameplayCoordinator::GetTrackedOpponent() const
{
	return m_tracked_opponent;
}

void GameplayCoordinator::SpawnRandomPickups()
{
	// Spawn pickups every 3 seconds using a fixed timer
	m_pickup_spawn_timer -= sf::seconds(1.f / 60.f);  // Assuming 60 FPS

	if (m_pickup_spawn_timer <= sf::Time::Zero)
	{
		m_pickup_spawn_timer = sf::seconds(3.f);  // Reset timer to 3 seconds

		// Compute view bounds directly
		sf::FloatRect view_bounds(m_camera.getCenter() - m_camera.getSize() / 2.f, m_camera.getSize());

		// Add a border margin to keep pickups away from edges
		const float border = 50.f;
		sf::FloatRect spawn_area(
			sf::Vector2f(view_bounds.position.x + border, view_bounds.position.y + border),
			sf::Vector2f(view_bounds.size.x - (border * 2.f), view_bounds.size.y - (border * 2.f))
		);

		// Random pickup type
		PickupType type = static_cast<PickupType>(Utility::RandomInt(static_cast<int>(PickupType::kPickupCount)));

		// Spawn at random position within camera view (with border)
		float random_x = spawn_area.position.x + Utility::RandomInt(static_cast<int>(spawn_area.size.x));
		float random_y = spawn_area.position.y + Utility::RandomInt(static_cast<int>(spawn_area.size.y));

		std::unique_ptr<Pickup> pickup(new Pickup(type, m_textures));
		pickup->setPosition(sf::Vector2f(random_x, random_y));
		pickup->SetVelocity(0.f, 0.f);

		m_upper_air_layer->AttachChild(std::move(pickup));
	}
}
