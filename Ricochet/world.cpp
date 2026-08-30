#include "world.hpp"
#include "sprite_node.hpp"
#include <iostream>
#include <random>
#include "state.hpp"
#include <SFML/System/Angle.hpp>
#include "Projectile.hpp"
#include "pickup.hpp"
#include "particle_node.hpp"
#include "particletype.hpp"
#include "sound_node.hpp"
#include "entity.hpp"
#include "collision_handler.hpp"
#include "gameplay_manager.hpp"
#include "physics_simulator.hpp"

World::World(sf::RenderTarget& output_target, FontHolder& font, SoundPlayer& sounds)
	: m_target(output_target)
	, m_camera(output_target.getDefaultView())
	, m_textures()
	, m_fonts(font)
	, m_sounds(sounds)
	, m_scene_graph(ReceiverCategories::kNone)
	, m_scene_layers()
	, m_world_bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(m_camera.getSize().x, m_camera.getSize().y))
	, m_spawn_position(Utility::RandomFloat(0.f, m_camera.getSize().x), Utility::RandomFloat(0.f, m_camera.getSize().y))
	, m_spawn_position_p2(sf::Vector2f(0.f, 0.f))
	, m_player_aircraft(nullptr)
	, m_player_aircraft_p2(nullptr)
	, m_player1_kill_display(nullptr)
	, m_player2_kill_display(nullptr)
	, m_tracked_opponent(nullptr)
	, m_pickup_spawn_timer(sf::seconds(0.f))
	, m_collision_handler(nullptr)
	, m_gameplay_manager(nullptr)
	, m_physics_simulator(nullptr)
{
	m_scene_texture.resize({ m_target.getSize().x, m_target.getSize().y });
	LoadTextures();

	BuildScene();
}

void World::Update(sf::Time dt)
{
	//UpdateSounds();

	//Process commands from the scenegraph
	while (!m_command_queue.IsEmpty())
	{
		m_scene_graph.OnCommand(m_command_queue.Pop(), dt);
	}

	// Use collision handler to process all collisions
	if (m_collision_handler)
	{
		m_collision_handler->HandleCollisions();
	}

	// Detect kills and handle respawns using gameplay manager
	if (m_gameplay_manager)
	{
		m_gameplay_manager->Update(m_player_aircraft, m_player_aircraft_p2);

		// Check if players died and respawn them
		bool player1_alive = m_player_aircraft && !m_player_aircraft->IsMarkedForRemoval();
		bool player2_alive = m_player_aircraft_p2 && !m_player_aircraft_p2->IsMarkedForRemoval();

		if (!player1_alive && m_player_aircraft)
		{
			m_player_aircraft->Respawn();
			m_player_aircraft->setPosition(m_spawn_position);
		}

		if (!player2_alive && m_player_aircraft_p2)
		{
			m_player_aircraft_p2->Respawn();
			m_player_aircraft_p2->setPosition(m_spawn_position_p2);
		}
	}

	m_scene_graph.RemoveWrecks();

	m_scene_graph.Update(dt, m_command_queue);

	SpawnRandomPickups();

	// Use physics simulator for physics updates
	if (m_physics_simulator)
	{
		m_physics_simulator->BounceProjectiles(m_command_queue);
		m_physics_simulator->HandlePlayerBoundaryCollision(m_player_aircraft, m_player_aircraft_p2);
	}
}

void World::Draw()
{
	if (PostEffect::IsSupported())
	{
		m_scene_texture.clear();
		m_scene_texture.setView(m_camera);
		m_scene_texture.draw(m_scene_graph);
		m_scene_texture.display();
		m_bloom_effect.Apply(m_scene_texture, m_target);
	}
	else
	{
		m_target.setView(m_camera);
		m_target.draw(m_scene_graph);
	}

	// Draw kill displays with default view (HUD layer - stays fixed on screen)
	m_target.setView(m_target.getDefaultView());
	if (m_player1_kill_display)
	{
		m_target.draw(*m_player1_kill_display);
	}
	if (m_player2_kill_display)
	{
		m_target.draw(*m_player2_kill_display);
	}
}

CommandQueue& World::GetCommandQueue()
{
	return m_command_queue;
}

bool World::HasAlivePlayer() const
{
	return m_player_aircraft && !m_player_aircraft->IsMarkedForRemoval();
}

int World::GetPlayer1Kills() const
{
	return m_gameplay_manager ? m_gameplay_manager->GetPlayer1Kills() : 0;
}

int World::GetPlayer2Kills() const
{
	return m_gameplay_manager ? m_gameplay_manager->GetPlayer2Kills() : 0;
}

void World::IncrementPlayer1Kills()
{
	if (m_gameplay_manager)
	{
		m_gameplay_manager->IncrementPlayer1Kills();
	}
}

void World::IncrementPlayer2Kills()
{
	if (m_gameplay_manager)
	{
		m_gameplay_manager->IncrementPlayer2Kills();
	}
}

void World::LoadTextures()
{
	m_textures.Load(TextureID::kEntities, "Media/Textures/Entities.png");
	m_textures.Load(TextureID::kExplosion, "Media/Textures/Explosion.png");
	
	m_textures.Load(TextureID::kBackground, "Media/Textures/Background.png");
	m_textures.Load(TextureID::kParticle, "Media/Textures/Particle.png");
}

void World::BuildScene()
{
	//Initialise the different layers
	for (int i = 0; i < static_cast<int>(SceneLayers::kLayerCount); i++)
	{
		ReceiverCategories category = (i == static_cast<int>(SceneLayers::kLowerAir)) ? ReceiverCategories::kScene : ReceiverCategories::kNone;
		SceneNode::Ptr layer(new SceneNode(category));
		m_scene_layers[i] = layer.get();
		m_scene_graph.AttachChild(std::move(layer));
	}

	//Prepare the background
	sf::Texture& texture = m_textures.Get(TextureID::kBackground);
	sf::IntRect textureRect(m_world_bounds);
	texture.setRepeated(true);

	//Add the background sprite to the world
	std::unique_ptr<SpriteNode> background_sprite(new SpriteNode(texture, textureRect));
	background_sprite->setPosition(sf::Vector2f(0.f, 0.f));
	m_background_sprite = background_sprite.get();
	m_scene_layers[static_cast<int>(SceneLayers::kBackground)]->AttachChild(std::move(background_sprite));

	std::unique_ptr<Aircraft> leader(new Aircraft(AircraftType::kEagle, m_textures, m_fonts, PlayerID::kPlayer1));
	m_player_aircraft = leader.get();
	m_player_aircraft->setPosition(m_spawn_position);
	m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(leader));

	// Spawn Player 2 at opposite side of the map
	// Mirror the spawn position: if P1 is at (x, y), P2 spawns at (width - x, height - y)
	m_spawn_position_p2 = sf::Vector2f(
		m_world_bounds.position.x + m_world_bounds.size.x - m_spawn_position.x,
		m_world_bounds.position.y + m_world_bounds.size.y - m_spawn_position.y
	);

	std::unique_ptr<Aircraft> player2(new Aircraft(AircraftType::kEagle, m_textures, m_fonts, PlayerID::kPlayer2));
	m_player_aircraft_p2 = player2.get();
	m_player_aircraft_p2->setPosition(m_spawn_position_p2);
	m_player_aircraft_p2->rotate(sf::degrees(180.f));  // Face opposite direction
	m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(player2));

	//Add the particle nodes to the scene
	std::unique_ptr<ParticleNode> smokeNode(new ParticleNode(ParticleType::kSmoke, m_textures));
	m_scene_layers[static_cast<int>(SceneLayers::kLowerAir)]->AttachChild(std::move(smokeNode));

	std::unique_ptr<ParticleNode> propellantNode(new ParticleNode(ParticleType::kPropellant, m_textures));
	m_scene_layers[static_cast<int>(SceneLayers::kLowerAir)]->AttachChild(std::move(propellantNode));

	KillGUI();

	//Add sound effect node
	std::unique_ptr<SoundNode> soundNode(new SoundNode(m_sounds));
	m_scene_graph.AttachChild(std::move(soundNode));

	// Initialize subsystems after players and UI are created
	m_collision_handler = std::make_unique<CollisionHandler>(m_player_aircraft, m_player_aircraft_p2, 
																  m_scene_graph, m_command_queue, m_sounds);
	m_gameplay_manager = std::make_unique<GameplayManager>(m_player_aircraft, m_player_aircraft_p2,
														   m_player1_kill_display.get(), m_player2_kill_display.get());
	m_physics_simulator = std::make_unique<PhysicsSimulator>(m_world_bounds, m_camera);
}

void World::KillGUI()
{
	// Create kill count displays (HUD elements - not attached to scene graph)
	std::string* p1_kill_text = new std::string("P1 Kills: 0");
	m_player1_kill_display = std::make_unique<TextNode>(m_fonts, *p1_kill_text);
	m_player1_kill_display->setPosition(sf::Vector2f(90.f, 30.f));
	m_player1_kill_display->setScale(sf::Vector2f(2.f, 2.f));
	m_player1_kill_display->SetColor(sf::Color::Blue);  // Player 1 - Blue

	std::string* p2_kill_text = new std::string("P2 Kills: 0");
	m_player2_kill_display = std::make_unique<TextNode>(m_fonts, *p2_kill_text);
	m_player2_kill_display->setPosition(sf::Vector2f(m_target.getSize().x - 90.f, 30.f));
	m_player2_kill_display->setScale(sf::Vector2f(2.f, 2.f));
	m_player2_kill_display->SetColor(sf::Color::Red);  // Player 2 - Red
}

void World::UpdateSounds()
{
	sf::Vector2f listener_position;

	listener_position = m_camera.getCenter();

	m_sounds.SetListenerPosition(listener_position);

	m_sounds.RemoveStoppedSounds();
}

void World::SpawnRandomPickups()
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

		m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(pickup));
	}
}

