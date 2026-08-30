#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "command_queue.hpp"
#include "resource_identifiers.hpp"

class Aircraft;
class SceneNode;
class CollisionHandler;
class GameplayManager;
class PhysicsSimulator;
class SoundPlayer;

class GameplayCoordinator
{
public:
	GameplayCoordinator(Aircraft* player1, Aircraft* player2, SceneNode& scene_graph,
		SceneNode* upper_air_layer, const sf::FloatRect& world_bounds, const sf::View& camera,
		CommandQueue& command_queue, TextureHolder& textures, SoundPlayer& sounds);

	~GameplayCoordinator() = default;

	// Main update loop for all gameplay subsystems
	void Update(sf::Time dt);

	// Kill tracking (delegates to GameplayManager)
	int GetPlayer1Kills() const;
	int GetPlayer2Kills() const;
	void IncrementPlayer1Kills();
	void IncrementPlayer2Kills();

	// Player respawning
	void RespawnDeadPlayers(const sf::Vector2f& spawn_pos_p1, const sf::Vector2f& spawn_pos_p2);

	// Tracked opponent accessor
	Aircraft* GetTrackedOpponent() const;

private:
	void GuideMissiles();
	void TrackPlayers();
	void SpawnRandomPickups();

private:
	// References to game objects
	Aircraft* m_player1;
	Aircraft* m_player2;
	SceneNode& m_scene_graph;
	SceneNode* m_upper_air_layer;
	const sf::FloatRect& m_world_bounds;
	const sf::View& m_camera;
	CommandQueue& m_command_queue;
	TextureHolder& m_textures;
	SoundPlayer& m_sounds;

	// Tracked opponent
	Aircraft* m_tracked_opponent;

	// Subsystems
	std::unique_ptr<CollisionHandler> m_collision_handler;
	std::unique_ptr<GameplayManager> m_gameplay_manager;
	std::unique_ptr<PhysicsSimulator> m_physics_simulator;

	// Pickup spawning timer
	sf::Time m_pickup_spawn_timer;
};
