#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <map>
#include "resource_identifiers.hpp"
#include "scene_node.hpp"
#include "scene_layers.hpp"
#include "aircraft.hpp"
#include "command_queue.hpp"
#include "bloom_effect.hpp"
#include "sound_player.hpp"
#include "sprite_node.hpp"
#include "text_node.hpp"
#include "collision_handler.hpp"
#include "gameplay_manager.hpp"
#include "physics_simulator.hpp"
#include "network_node.hpp"
#include "gameplay_coordinator.hpp"
#include "pickup.hpp"

class KeyBinding;

class World
{
public:
	explicit World(sf::RenderTarget& output_target, FontHolder& font, SoundPlayer& sounds, const KeyBinding* key_binding = nullptr);
	void Update(sf::Time dt);
	void Draw();

	CommandQueue& GetCommandQueue();

	bool HasAlivePlayer() const;

	void SpawnRandomPickups(sf::Time dt);

	// Multiplayer aircraft management
	Aircraft* GetAircraft(uint8_t aircraft_id);
	Aircraft* AddAircraft(uint8_t aircraft_id, PlayerID player_id = PlayerID::kPlayer1);
	void RemoveAircraft(uint8_t aircraft_id);
	void SetupNetworkNode();

	// Gameplay management access
	GameplayManager* GetGameplayManager();

	bool PollGameAction(GameActions::Action& out);

	// Network pickup spawning
	void SetIsHost(bool is_host);
	void SpawnPickupFromNetwork(uint32_t pickup_id, int pickup_type, sf::Vector2f position);
	void SetPickupBroadcasterCallback(std::function<void(uint32_t, int, sf::Vector2f)> callback);  // Callback to send pickup spawns to clients (id, type, position)
	void SetPickupCollectedCallback(std::function<void(uint32_t)> callback);  // Callback when pickup is collected (forward to server)

	// Pickup management
	void CreatePickup(PickupID id, PickupType type, sf::Vector2f position);
	void RemovePickup(PickupID id);

private:
	void LoadTextures();
	void BuildScene();

	void UpdateSounds();

	// Spawn point management
	void GenerateSpawnPoints();
	sf::Vector2f GetNextSpawnPoint();

	// Initialize GameplayCoordinator on first aircraft spawn
	void InitializeGameplayCoordinator();

private:
	sf::RenderTarget& m_target;
	sf::RenderTexture m_scene_texture;
	sf::View m_camera;
	TextureHolder m_textures;
	FontHolder& m_fonts;
	SoundPlayer& m_sounds;
	const KeyBinding* m_key_binding;
	SceneNode m_scene_graph;
	std::array<SceneNode*, static_cast<int>(SceneLayers::kLayerCount)> m_scene_layers;
	sf::FloatRect m_world_bounds;

	CommandQueue m_command_queue;
	Command m_command;

	BloomEffect m_bloom_effect;
	SpriteNode* m_background_sprite;
	sf::Time m_pickup_spawn_timer;
	bool m_is_host;  // Track if this World instance is running on the host
	std::function<void(uint32_t, int, sf::Vector2f)> m_pickup_broadcaster;  // Callback to broadcast pickups to clients (id, type, position)
	std::function<void(uint32_t)> m_pickup_collected_callback;  // Callback when pickup is collected (forward to server)

	// Subsystems
	std::unique_ptr<CollisionHandler> m_collision_handler;
	std::unique_ptr<GameplayManager> m_gameplay_manager;
	std::unique_ptr<PhysicsSimulator> m_physics_simulator;
	std::unique_ptr<GameplayCoordinator> m_gameplay_coordinator;

	// Multiplayer aircraft management - Player 1 = ID 0, Player 2 = ID 1
	std::map<uint8_t, Aircraft*> m_networked_aircraft;
	std::map<uint8_t, TextNode*> m_player_kill_displays;  // Track kill display GUIs
	std::vector<Aircraft*> m_players_list;  // Maintained list for GameplayCoordinator
	NetworkNode* m_network_node;

	std::map<PickupID, Pickup*> m_pickups;

	// Dynamic spawn point management
	std::vector<sf::Vector2f> m_spawn_points;
	size_t m_next_spawn_point_index;
};