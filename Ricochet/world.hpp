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

class World
{
public:
	explicit World(sf::RenderTarget& output_target, FontHolder& font, SoundPlayer& sounds);
	void Update(sf::Time dt);
	void Draw();

	CommandQueue& GetCommandQueue();

	bool HasAlivePlayer() const;

	// Kill tracking
	int GetPlayer1Kills() const;
	int GetPlayer2Kills() const;
	void IncrementPlayer1Kills();
	void IncrementPlayer2Kills();

private:
	void LoadTextures();
	void BuildScene();

	void KillGUI();

	void UpdateSounds();
	void SpawnRandomPickups();

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
	sf::Vector2f m_spawn_position;
	sf::Vector2f m_spawn_position_p2;
	Aircraft* m_player_aircraft;
	Aircraft* m_player_aircraft_p2;

	// Kill count UI
	std::unique_ptr<TextNode> m_player1_kill_display;
	std::unique_ptr<TextNode> m_player2_kill_display;

	// Tracked opponent
	Aircraft* m_tracked_opponent;

	CommandQueue m_command_queue;
	Command m_command;

	BloomEffect m_bloom_effect;
	SpriteNode* m_background_sprite;
	sf::Time m_pickup_spawn_timer;

	// Subsystems
	std::unique_ptr<CollisionHandler> m_collision_handler;
	std::unique_ptr<GameplayManager> m_gameplay_manager;
	std::unique_ptr<PhysicsSimulator> m_physics_simulator;
};

