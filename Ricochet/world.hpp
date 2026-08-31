#pragma once
#include <SFML/Graphics.hpp>
#include "resource_identifiers.hpp"
#include "scene_node.hpp"
#include "scene_layers.hpp"
#include "aircraft.hpp"
#include "command_queue.hpp"
#include "bloom_effect.hpp"
#include "sound_player.hpp"
#include "sprite_node.hpp"
#include "text_node.hpp"
#include "utility.hpp"
#include "collision_handler.hpp"
#include "gameplay_manager.hpp"
#include "physics_simulator.hpp"
#include "network_node.hpp"

class World
{
public:
	explicit World(sf::RenderTarget& output_target, FontHolder& font, SoundPlayer& sounds);
	void Update(sf::Time dt);
	void Draw();

	CommandQueue& GetCommandQueue();

	bool HasAlivePlayer() const;

	void SpawnRandomPickups();

	// Multiplayer aircraft management
	Aircraft* GetAircraft(uint8_t aircraft_id);
	Aircraft* AddAircraft(uint8_t aircraft_id, PlayerID player_id = PlayerID::kPlayer1);
	void RemoveAircraft(uint8_t aircraft_id);
	void SetupNetworkNode();

	bool PollGameAction(GameActions::Action& out);

private:
	void LoadTextures();
	void BuildScene();

	void UpdateSounds();

	// Spawn point management
	void GenerateSpawnPoints();
	sf::Vector2f GetNextSpawnPoint();
	


private:
	sf::RenderTarget& m_target;
	sf::RenderTexture m_scene_texture;
	sf::View m_camera;
	TextureHolder m_textures;
	FontHolder& m_fonts;
	SoundPlayer& m_sounds;
	SceneNode m_scene_graph;
	std::array<SceneNode*, static_cast<int>(SceneLayers::kLayerCount)> m_scene_layers;
	sf::FloatRect m_world_bounds;

	CommandQueue m_command_queue;
	Command m_command;

	BloomEffect m_bloom_effect;
	SpriteNode* m_background_sprite;
	sf::Time m_pickup_spawn_timer;

	// Subsystems
	std::unique_ptr<CollisionHandler> m_collision_handler;
	std::unique_ptr<GameplayManager> m_gameplay_manager;
	std::unique_ptr<PhysicsSimulator> m_physics_simulator;

	// Multiplayer aircraft management - Player 1 = ID 0, Player 2 = ID 1
	std::map<uint8_t, Aircraft*> m_networked_aircraft;
	NetworkNode* m_network_node;

	// Dynamic spawn point management
	std::vector<sf::Vector2f> m_spawn_points;
	size_t m_next_spawn_point_index;
};