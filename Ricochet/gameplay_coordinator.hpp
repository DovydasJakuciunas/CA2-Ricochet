#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include "command_queue.hpp"
#include "resource_identifiers.hpp"
#include "pickup_type.hpp"

class Aircraft;
class SceneNode;
class CollisionHandler;
class GameplayManager;
class PhysicsSimulator;
class SoundPlayer;
class TextNode;

class GameplayCoordinator
{
public:
	GameplayCoordinator(std::vector<Aircraft*>& players, SceneNode& scene_graph,
		SceneNode* upper_air_layer, const sf::FloatRect& world_bounds, const sf::View& camera,
		CommandQueue& command_queue, TextureHolder& textures, SoundPlayer& sounds);

	~GameplayCoordinator() = default;

	// Main update loop for all gameplay subsystems
	void Update(sf::Time dt);

	// Player respawning
	void RespawnDeadPlayers(const std::vector<sf::Vector2f>& spawn_positions);

	// Register a player's kill display UI
	void RegisterPlayerKillDisplay(uint8_t playerID, TextNode* kill_display);

	// Set whether this coordinator is running on the host (for collision handling)
	void SetIsHost(bool is_host);

private:
	// References to game objects
	std::vector<Aircraft*>& m_players;
	SceneNode& m_scene_graph;
	SceneNode* m_upper_air_layer;
	const sf::FloatRect& m_world_bounds;
	const sf::View& m_camera;
	CommandQueue& m_command_queue;
	TextureHolder& m_textures;
	SoundPlayer& m_sounds;

	// Subsystems
	std::unique_ptr<CollisionHandler> m_collision_handler;
	std::unique_ptr<GameplayManager> m_gameplay_manager;
	std::unique_ptr<PhysicsSimulator> m_physics_simulator;

	// Pickup spawning timer
	sf::Time m_pickup_spawn_timer;
};
