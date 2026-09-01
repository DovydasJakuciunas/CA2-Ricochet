#include "world.hpp"
#include "sprite_node.hpp"
#include <vector>
#include <algorithm>
#include <iostream>
#include "pickup.hpp"
#include "particle_node.hpp"
#include "particletype.hpp"
#include "sound_node.hpp"
#include "entity.hpp"
#include "collision_handler.hpp"
#include "gameplay_manager.hpp"
#include "key_binding.hpp"
#include "physics_simulator.hpp"
#include "gameplay_coordinator.hpp"

World::World(sf::RenderTarget& output_target, FontHolder& font, SoundPlayer& sounds, const KeyBinding* key_binding)
	: m_target(output_target)
	, m_camera(output_target.getDefaultView())
	, m_textures()
	, m_fonts(font)
	, m_sounds(sounds)
	, m_key_binding(key_binding)
	, m_scene_graph(ReceiverCategories::kNone)
	, m_scene_layers()
	, m_world_bounds(sf::Vector2f(0.f, 0.f), sf::Vector2f(1024.f, 768.f))
	, m_pickup_spawn_timer(sf::seconds(0.f))
	, m_collision_handler(nullptr)
	, m_gameplay_manager(nullptr)
	, m_physics_simulator(nullptr)
	, m_gameplay_coordinator(nullptr)
	, m_network_node(nullptr)
	, m_next_spawn_point_index(0)
{
	m_scene_texture.resize({ m_target.getSize().x, m_target.getSize().y });
	LoadTextures();
	GenerateSpawnPoints();

	BuildScene();
	SetupNetworkNode();

	// GameplayCoordinator will be initialized on first aircraft spawn via InitializeGameplayCoordinator()
}

void World::Update(sf::Time dt)
{
	//Process commands from the scenegraph
	while (!m_command_queue.IsEmpty())
	{
		m_scene_graph.OnCommand(m_command_queue.Pop(), dt);
	}

	// Update scene graph first to move all entities based on their velocity
	m_scene_graph.Update(dt, m_command_queue);

	// Remove dead aircraft from the players list BEFORE RemoveWrecks() to prevent dangling pointers
	m_players_list.erase(
		std::remove_if(m_players_list.begin(), m_players_list.end(),
			[](Aircraft* aircraft) { return !aircraft || aircraft->IsMarkedForRemoval(); }),
		m_players_list.end()
	);

	// Check if players died and respawn them (BEFORE RemoveWrecks to ensure pointers are still valid)
	for (auto& pair : m_networked_aircraft)
	{
		Aircraft* player = pair.second;
		if (player && player->IsMarkedForRemoval())
		{
			// Award a point to whoever last damaged this player before respawning
			if (GameplayManager* gameplay_manager = GetGameplayManager())
			{
				gameplay_manager->OnPlayerDeath(static_cast<uint8_t>(player->GetPlayerID()));
			}

			player->Respawn();
			player->setPosition(GetNextSpawnPoint());
			m_players_list.push_back(player);  // Re-add respawned player to collision handling
		}
	}

	m_scene_graph.RemoveWrecks();

	// Use GameplayCoordinator for all gameplay updates (collision handling, physics, etc.)
	if (m_gameplay_coordinator)
	{
		m_gameplay_coordinator->Update(dt);
	}

	// Clean up dead aircraft from networked_aircraft map AFTER respawn loop to prevent dangling pointers
	for (auto itr = m_networked_aircraft.begin(); itr != m_networked_aircraft.end();)
	{
		if (!itr->second || itr->second->IsMarkedForRemoval())
		{
			itr = m_networked_aircraft.erase(itr);
		}
		else
		{
			++itr;
		}
	}

	SpawnRandomPickups(dt);
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
}

CommandQueue& World::GetCommandQueue()
{
	return m_command_queue;
}

bool World::HasAlivePlayer() const
{
	// Check if player 1 (aircraft ID 1) is alive
	// Note: aircraft_id starts at 1, not 0
	auto it = m_networked_aircraft.find(1);
	return it != m_networked_aircraft.end() && it->second && !it->second->IsMarkedForRemoval();
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

	//Add the particle nodes to the scene
	std::unique_ptr<ParticleNode> smokeNode(new ParticleNode(ParticleType::kSmoke, m_textures));
	m_scene_layers[static_cast<int>(SceneLayers::kLowerAir)]->AttachChild(std::move(smokeNode));

	std::unique_ptr<ParticleNode> propellantNode(new ParticleNode(ParticleType::kPropellant, m_textures));
	m_scene_layers[static_cast<int>(SceneLayers::kLowerAir)]->AttachChild(std::move(propellantNode));

	//Add sound effect node
	std::unique_ptr<SoundNode> soundNode(new SoundNode(m_sounds));
	m_scene_graph.AttachChild(std::move(soundNode));

	m_physics_simulator = std::make_unique<PhysicsSimulator>(m_world_bounds, m_camera);
}

void World::SetupNetworkNode()
{
	if (!m_network_node)
	{
		std::unique_ptr<NetworkNode> network_node(new NetworkNode());
		m_network_node = network_node.get();
		m_scene_graph.AttachChild(std::move(network_node));
	}
}

void World::UpdateSounds()
{
	sf::Vector2f listener_position;

	listener_position = m_camera.getCenter();

	m_sounds.SetListenerPosition(listener_position);

	m_sounds.RemoveStoppedSounds();
}

void World::SpawnRandomPickups(sf::Time dt)
{
	// Only the host should spawn random pickups in multiplayer
	if (!m_is_host)
		return;

	// Spawn pickups every 3 seconds using actual frame delta time
	m_pickup_spawn_timer -= dt;

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

		// Validate spawn area is large enough before attempting to spawn pickups
		if (spawn_area.size.x <= 0.f || spawn_area.size.y <= 0.f)
		{
			return;  // Camera view is too small; skip pickup spawning this frame
		}

		// Random pickup type
		PickupType type = static_cast<PickupType>(Utility::RandomInt(static_cast<int>(PickupType::kPickupCount)));

		// Spawn at random position within camera view (with border)
		float random_x = spawn_area.position.x + Utility::RandomInt(static_cast<int>(spawn_area.size.x));
		float random_y = spawn_area.position.y + Utility::RandomInt(static_cast<int>(spawn_area.size.y));

		PickupID id = Utility::RandomInt(std::numeric_limits<PickupID>::max());
		auto pickup = std::make_unique<Pickup>(id, type, m_textures);
		pickup->setPosition(sf::Vector2f(random_x, random_y));
		pickup->SetVelocity(0.f, 0.f);

		// Register pickup before attaching to scene
		m_pickups[id] = pickup.get();

		// Broadcast pickup spawn to all clients
		if (m_pickup_broadcaster)
		{
			m_pickup_broadcaster(id, static_cast<int>(type), sf::Vector2f(random_x, random_y));
		}

		m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(pickup));
	}
}

void World::SetIsHost(bool is_host)
{
	m_is_host = is_host;
	// Also update the GameplayCoordinator's collision handler if it exists
	if (m_gameplay_coordinator)
	{
		m_gameplay_coordinator->SetIsHost(is_host);
	}
}

void World::SetPickupBroadcasterCallback(std::function<void(uint32_t, int, sf::Vector2f)> callback)
{
	m_pickup_broadcaster = callback;
}

void World::SetPickupCollectedCallback(std::function<void(uint32_t)> callback)
{
	m_pickup_collected_callback = callback;
	// Also set it on the GameplayCoordinator if it exists
	if (m_gameplay_coordinator)
	{
		m_gameplay_coordinator->SetPickupCollectedCallback(callback);
	}
}

// Multiplayer aircraft management methods
Aircraft* World::GetAircraft(uint8_t aircraft_id)
{
	auto it = m_networked_aircraft.find(aircraft_id);
	if (it != m_networked_aircraft.end())
	{
		return it->second;
	}
	return nullptr;
}

Aircraft* World::AddAircraft(uint8_t aircraft_id, PlayerID player_id)
{
	// Create a new aircraft with default type (Eagle) and the specified player ID
	std::unique_ptr<Aircraft> aircraft(new Aircraft(AircraftType::kEagle, m_textures, m_fonts, player_id));
	Aircraft* aircraft_ptr = aircraft.get();

	// Set key binding for animation if available
	if (m_key_binding)
	{
		aircraft_ptr->SetKeyBinding(m_key_binding);
	}

	// Store in the map
	m_networked_aircraft[aircraft_id] = aircraft_ptr;

	// Add to players list for collision handling
	m_players_list.push_back(aircraft_ptr);

	// Initialize GameplayCoordinator on first aircraft spawn
	if (!m_gameplay_coordinator)
	{
		InitializeGameplayCoordinator();
	}

	// Create kill display TextNode for this player
	std::string kill_text("");
	std::unique_ptr<TextNode> kill_display(new TextNode(m_fonts, kill_text));
	TextNode* kill_display_ptr = kill_display.get();

	// Position the kill display vertically along the left side of the screen
	const float box_height = 70.f;
	const float screen_top = 7.5f;
	const float screen_bottom = 768.f - box_height;  // Account for entire box height
	const float spacing_between_players = 75.f;  // Fixed spacing between players

	// Calculate Y position and clamp to screen bounds
	float y_position = screen_top + (aircraft_id * spacing_between_players);
	y_position = std::min(y_position, screen_bottom);

	kill_display_ptr->setPosition(sf::Vector2f(150.f, y_position));

	// Set larger font size for visibility
	kill_display_ptr->SetCharacterSize(42);

	// Add border/outline around text
	kill_display_ptr->SetOutlineThickness(2.f);
	kill_display_ptr->SetOutlineColor(sf::Color::Black);

	// Add background box with 10 pixel padding
	kill_display_ptr->SetBackgroundPadding(10.f);
	kill_display_ptr->SetBackgroundColor(sf::Color(0, 0, 0, 100));  // Semi-transparent black
	kill_display_ptr->SetBackgroundOutlineColor(sf::Color::White);
	kill_display_ptr->SetBackgroundOutlineThickness(2.f);

	// Store reference
	m_player_kill_displays[aircraft_id] = kill_display_ptr;

	// Attach to scene graph (GUI layer - renders on top, in front of all other elements)
	m_scene_layers[static_cast<int>(SceneLayers::kGUI)]->AttachChild(std::move(kill_display));

	// Register the kill display with GameplayManager via GameplayCoordinator
	if (m_gameplay_coordinator)
	{
		uint8_t player_index = aircraft_id - 1;  // Convert 1-based aircraft_id to 0-based player index
		m_gameplay_coordinator->RegisterPlayerKillDisplay(player_index, kill_display_ptr);
	}

	// Give newly spawned aircraft collision immunity for a moment so they don't immediately take damage
	aircraft_ptr->SetCollisionImmunity(sf::milliseconds(1000));

	// Add to scene graph
	m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(aircraft));

	return aircraft_ptr;
}

void World::RemoveAircraft(uint8_t aircraft_id)
{
	auto it = m_networked_aircraft.find(aircraft_id);
	if (it != m_networked_aircraft.end())
	{
		// Immediately remove the aircraft from the scene graph
		if (it->second)
		{
			// Get the parent and detach the aircraft immediately
			SceneNode* parent = it->second->GetParent();
			if (parent)
			{
				parent->DetachChild(*it->second);
			}

			// Remove from players list
			auto player_it = std::find(m_players_list.begin(), m_players_list.end(), it->second);
			if (player_it != m_players_list.end())
			{
				m_players_list.erase(player_it);
			}
		}
		m_networked_aircraft.erase(it);

		// Clean up kill display - detach TextNode from scene graph
		auto display_it = m_player_kill_displays.find(aircraft_id);
		if (display_it != m_player_kill_displays.end())
		{
			if (display_it->second)
			{
				SceneNode* text_parent = display_it->second->GetParent();
				if (text_parent)
				{
					text_parent->DetachChild(*display_it->second);
				}
			}
			m_player_kill_displays.erase(display_it);
		}
	}
}

bool World::PollGameAction(GameActions::Action& out)
{
	return m_network_node->PollGameAction(out);
}

GameplayManager* World::GetGameplayManager()
{
	// The active gameplay manager lives inside the GameplayCoordinator
	if (m_gameplay_coordinator)
	{
		return m_gameplay_coordinator->GetGameplayManager();
	}
	return m_gameplay_manager.get();
}

void World::GenerateSpawnPoints()
{
	m_spawn_points.clear();
	m_next_spawn_point_index = 0;

	// Generate spawn points in a grid pattern with spacing
	const float spacing = 150.f;  // Minimum distance between spawn points
	const float margin = 50.f;     // Margin from world edges

	float x_start = m_world_bounds.position.x + margin;
	float y_start = m_world_bounds.position.y + margin;
	float x_end = m_world_bounds.position.x + m_world_bounds.size.x - margin;
	float y_end = m_world_bounds.position.y + m_world_bounds.size.y - margin;

	// Generate grid of spawn points
	for (float y = y_start; y < y_end; y += spacing)
	{
		for (float x = x_start; x < x_end; x += spacing)
		{
			m_spawn_points.push_back(sf::Vector2f(x, y));
		}
	}
}

sf::Vector2f World::GetNextSpawnPoint()
{
	if (m_spawn_points.empty())
	{
		// Fallback to center if no spawn points generated
		return sf::Vector2f(m_world_bounds.position.x + m_world_bounds.size.x / 2.f,
						   m_world_bounds.position.y + m_world_bounds.size.y / 2.f);
	}

	// Get next spawn point and cycle through
	sf::Vector2f spawn_pos = m_spawn_points[m_next_spawn_point_index];
	m_next_spawn_point_index = (m_next_spawn_point_index + 1) % m_spawn_points.size();

	return spawn_pos;
}

void World::InitializeGameplayCoordinator()
{
	if (!m_gameplay_coordinator)
	{
		m_gameplay_coordinator = std::make_unique<GameplayCoordinator>(
			m_players_list,
			m_scene_graph,
			m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)],
			m_world_bounds,
			m_camera,
			m_command_queue,
			m_textures,
			m_sounds
		);

		// Set pickup collected callback if one was registered
		if (m_pickup_collected_callback)
		{
			m_gameplay_coordinator->SetPickupCollectedCallback(m_pickup_collected_callback);
		}
	}
}

// Pickups
void World::CreatePickup(
	PickupID id,
	PickupType type,
	sf::Vector2f position)
{
	if (m_pickups.contains(id))
		return;

	auto pickup = std::make_unique<Pickup>(id, type, m_textures);
	pickup->setPosition(position);

	m_pickups[id] = pickup.get();
	m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]
		->AttachChild(std::move(pickup));
}

void World::RemovePickup(PickupID id)
{
	auto found = m_pickups.find(id);
	if (found == m_pickups.end())
		return;

	found->second->Destroy();
	m_pickups.erase(found);
}

void World::SpawnPickupFromNetwork(uint32_t pickup_id, int pickup_type, sf::Vector2f position)
{
	// Clients spawn pickups received from the server
	PickupType type = static_cast<PickupType>(pickup_type);
	PickupID id = pickup_id;  // Use the ID sent from the server
	auto pickup = std::make_unique<Pickup>(id, type, m_textures);
	pickup->setPosition(position);
	pickup->SetVelocity(0.f, 0.f);

	// Register pickup before attaching to scene
	m_pickups[id] = pickup.get();

	m_scene_layers[static_cast<int>(SceneLayers::kUpperAir)]->AttachChild(std::move(pickup));
}
