#include "multiplayer_gamestate.hpp"
#include "music_player.hpp"
#include "utility.hpp"
#include "fontID.hpp"
#include "weapon_system.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Network/Packet.hpp>
#include <SFML/Network/IpAddress.hpp>

#include <fstream>
#include "pickup_type.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

std::optional<sf::IpAddress> MultiplayerGameState::GetAddressFromFile()
{
	std::ifstream inputFile("ip.txt");

	if (!inputFile)
	{
		return std::nullopt;
	}

	std::string ipString;
	inputFile >> ipString;

	auto address = sf::IpAddress::resolve(ipString);

	if (!address)
	{
		return std::nullopt;
	}

	return address;
}

MultiplayerGameState::MultiplayerGameState(StateStack& stack, Context context, bool is_host)
	:State(stack, context)
	, m_world(*context.window, *context.fonts, *context.sound, context.keys1)
	, m_window(*context.window)
	, m_texture_holder(*context.textures)
	, m_connected(false)
	, m_game_server(nullptr)
	, m_active_state(true)
	, m_has_focus(true)
	, m_host(is_host)
	, m_game_started(false)
	, m_client_timeout(sf::seconds(5.f))
	, m_time_since_last_packet(sf::seconds(0.f))
	, m_broadcast_text(context.fonts->Get(FontID::kMain))
	, m_player_invitation_text(context.fonts->Get(FontID::kMain))
	, m_failed_connection_text(context.fonts->Get(FontID::kMain))
	, m_network_stats_text(context.fonts->Get(FontID::kMain))
	, m_pause_text(context.fonts->Get(FontID::kMain))
{
	m_broadcast_text.setPosition(sf::Vector2f(1024.f / 2, 100.f));

	// Initialize network stats display text
	m_network_stats_text.setCharacterSize(14);
	m_network_stats_text.setFillColor(sf::Color::Green);
	m_network_stats_text.setPosition(sf::Vector2f(10.f, 10.f));
	m_network_stats_text.setString("Loading network stats...");

	// Inform the World whether this instance is host or client
	m_world.SetIsHost(m_host);

	// Setup pickup broadcaster callback for host
	if (m_host)
	{
		m_world.SetPickupBroadcasterCallback([this](uint32_t pickup_id, int pickup_type, sf::Vector2f position)
		{
			if (m_game_server)
			{
				m_game_server->NotifyPickupSpawn(pickup_id, pickup_type, position);
			}
		});

		// Setup pickup collected callback for host to notify clients
		m_world.SetPickupCollectedCallback([this](uint32_t pickup_id)
		{
			if (m_game_server)
			{
				m_game_server->NotifyPickupCollected(pickup_id);
			}
		});
	}

	//Use this for "Attempt to connect" and "Failed to connect" messages
	m_failed_connection_text.setCharacterSize(35);
	m_failed_connection_text.setFillColor(sf::Color::White);
	m_failed_connection_text.setString("Attempting to connect...");
	Utility::CentreOrigin(m_failed_connection_text);
	m_failed_connection_text.setPosition(sf::Vector2f(m_window.getSize().x / 2.f, m_window.getSize().y / 2.f));

	//Render an establishing connection frame for user feedback
	m_window.clear(sf::Color::Black);
	m_window.draw(m_failed_connection_text);
	m_window.display();
	m_failed_connection_text.setString("Failed to connect to server");
	Utility::CentreOrigin(m_failed_connection_text);

	//If this is the host, create a server
	std::optional<sf::IpAddress> ip;

	if (m_host)
	{
		m_game_server.reset(new GameServer(sf::Vector2f(m_window.getSize())));
		ip = sf::IpAddress::LocalHost;
	}
	else
	{
		ip = GetAddressFromFile();
	}

	if (ip)
	{
		auto status = m_socket.connect(*ip, SERVER_PORT, sf::seconds(5.f));

		if (status == sf::Socket::Status::Done)
		{
			m_connected = true;
			// Set non-blocking ONLY after a successful connection
			m_socket.setBlocking(false);
		}
		else
		{
			m_failed_connection_clock.restart();
		}
	}
	else
	{
		m_failed_connection_clock.restart();
	}

	//Set socket to non-blocking
	m_socket.setBlocking(false);

	// Wait for initial packets from server (kSpawnSelf and kInitialState)
	if (m_connected)
	{
		sf::Clock wait_clock;
		bool received_spawn_self = false;

		while (wait_clock.getElapsedTime() < sf::seconds(5.f) && !received_spawn_self)
		{
			sf::Packet packet;
			if (m_socket.receive(packet) == sf::Socket::Status::Done)
			{
				m_time_since_last_packet = sf::seconds(0.f);
				uint8_t packet_type;
				packet >> packet_type;
				HandlePacket(packet_type, packet);

				if (packet_type == static_cast<uint8_t>(Server::PacketType::kSpawnSelf))
				{
					received_spawn_self = true;
				}
			}
					// Small sleep to prevent busy-waiting
					sf::sleep(sf::milliseconds(10));
				}
			}

				// Initialize pause overlay text
				m_pause_text.setFont(context.fonts->Get(FontID::kMain));
				m_pause_text.setString("Paused\nPress ESC to Resume\nPress Backspace to Exit to Menu");
				m_pause_text.setCharacterSize(40);
				m_pause_text.setFillColor(sf::Color::White);
				Utility::CentreOrigin(m_pause_text);
				m_pause_text.setPosition(sf::Vector2f(m_window.getSize().x / 2.f, m_window.getSize().y / 2.f));

				//Play the game music
	context.music->Play(MusicThemes::kMissionTheme);
}

void MultiplayerGameState::Draw()
{
	if (m_connected)
	{
		m_world.Draw();

		//Show the broadcast message in default view
		m_window.setView(m_window.getDefaultView());

		if (!m_broadcasts.empty())
		{
			m_window.draw(m_broadcast_text);
		}

		// Draw network statistics overlay
		if (m_show_stats)
		{
			m_window.draw(m_network_stats_text);
		}

		// Draw pause overlay if paused
		if (m_paused)
		{
			sf::RectangleShape pause_overlay;
			pause_overlay.setFillColor(sf::Color(0, 0, 0, 150));
			pause_overlay.setSize(m_window.getView().getSize());
			m_window.draw(pause_overlay);
			m_window.draw(m_pause_text);
		}
	}
	else
	{
		m_window.draw(m_failed_connection_text);
	}
}

bool MultiplayerGameState::Update(sf::Time dt)
{
	//Connected to the Server: Handle all the network logic
	if (m_connected)
	{
		m_world.Update(dt);

		// Host: Check for recently disconnected aircraft and clean up their GUI
		if (m_host && m_game_server)
		{
			std::vector<uint8_t> disconnected = m_game_server->GetAndClearRecentlyDisconnectedAircraft();
			for (uint8_t aircraft_identifier : disconnected)
			{
				m_world.RemoveAircraft(aircraft_identifier);
			}
		}

		for (auto itr = m_players.begin(); itr != m_players.end();)
		{
			if (!m_world.GetAircraft(itr->first))
			{
				itr = m_players.erase(itr);
			}
			else
			{
				++itr;
			}
		}

		//Win condition: first player to reach kKillsToWin kills wins the game.
		//Only the host (authoritative) checks the score and then broadcasts
		//kMissionSuccess to every client so they all see the Game Over screen.
		if (m_host && m_game_server && m_game_started && !m_mission_over)
		{
			if (GameplayManager* gameplay_manager = m_world.GetGameplayManager())
			{
				for (const auto& [playerID, kills] : gameplay_manager->GetAllPlayerKills())
				{
					if (kills >= kKillsToWin)
					{
						m_mission_over = true;
						m_game_server->NotifyMissionSuccess();
						break;
					}
				}
			}
		}

		//Handle all realtime input for players with correct PlayerID
		CommandQueue& commands = m_world.GetCommandQueue();
		if (m_active_state && m_has_focus && !m_paused)
		{
			for (auto& pair : m_players)
			{
				pair.second->HandleRealTimeInput(commands);
				pair.second->HandleRealtimeNetworkInput(commands);
			}
		}

		sf::Packet packet;
		bool received_any = false;
		while (m_socket.receive(packet) == sf::Socket::Status::Done)
		{
			received_any = true;
			m_time_since_last_packet = sf::seconds(0.f);
			m_bytes_received += packet.getDataSize();
			uint8_t packet_type;
			packet >> packet_type;
			HandlePacket(packet_type, packet);
			packet.clear();
		}

		if (!received_any)
		{
			//Check for timeout with the server
			if (m_time_since_last_packet > m_client_timeout)
			{
				m_connected = false;
				m_failed_connection_text.setString("Lost connection to the server");
				Utility::CentreOrigin(m_failed_connection_text);

				m_failed_connection_clock.restart();
			}
		}

		UpdateBroadcastMessage(dt);

		//Events occurring in the game
		GameActions::Action game_action;
		while (m_world.PollGameAction(game_action))	//Removes next action from the queue and returns true if there was an action
		{
			sf::Packet packet;
			packet << static_cast<uint8_t>(Client::PacketType::kGameEvent);
			packet << static_cast<uint8_t>(game_action.type);
			packet << game_action.position.x;
			packet << game_action.position.y;

			m_socket.send(packet);
			m_bytes_sent += packet.getDataSize();
		}

		if (m_state_update_clock.getElapsedTime() > sf::seconds(1.f / 10.f))
		{
			// The server is authoritative over positions and simulates movement
			// from the input commands we send. We intentionally do NOT report our
			// own positions back to the server; doing so would let each owner
			// decide where its aircraft is. The clock is still restarted so the
			// heartbeat cadence below behaves as before.
			m_state_update_clock.restart();
		}

		// Send heartbeat to prevent timeout during idle periods (~1 Hz, only if no recent position updates)
		if (m_heartbeat_clock.getElapsedTime() > sf::seconds(1.f))
		{
			sf::Packet heartbeat_packet;
			heartbeat_packet << static_cast<uint8_t>(Client::PacketType::kHeartbeat);
			m_socket.send(heartbeat_packet);
			m_bytes_sent += heartbeat_packet.getDataSize();
			m_heartbeat_clock.restart();
		}

		// Update network statistics display every 500ms
		if (m_stats_update_clock.getElapsedTime() > sf::milliseconds(500))
		{
			if (m_game_server)
			{
				m_current_stats = m_game_server->GetNetworkStats();
			}

			// Format the stats text
			std::stringstream ss;
			ss << "=== Network Statistics ===\n";
			ss << "Connected Players: " << static_cast<int>(m_current_stats.connected_players) << "\n";
			ss << "Packets Sent: " << m_current_stats.packets_sent << "\n";
			ss << "Packets Received: " << m_current_stats.packets_received << "\n";
			ss << "Bytes Sent: " << NetworkStats::FormatBytes(m_current_stats.bytes_sent) << "\n";
			ss << "Bytes Received: " << NetworkStats::FormatBytes(m_current_stats.bytes_received) << "\n";
			ss << "[Press 'S' to toggle]\n";

			m_network_stats_text.setString(ss.str());
			m_stats_update_clock.restart();
		}

		m_time_since_last_packet += dt;
	}

	//Failed to connect and waited for more than 5 seconds: Back to menu
	else if (m_failed_connection_clock.getElapsedTime() >= sf::seconds(5.f))
	{
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
	}
	return true;
}

bool MultiplayerGameState::HandleEvent(const sf::Event& event)
{
	//Game input handling
	CommandQueue& commands = m_world.GetCommandQueue();

	//Forward events to all players
	for (auto& pair : m_players)
	{
		pair.second->HandleEvent(event, commands);
	}
	const auto* key_pressed = event.getIf<sf::Event::KeyPressed>();
	if (key_pressed)
	{
		//If escape is pressed, toggle pause
		if (key_pressed->scancode == sf::Keyboard::Scancode::Escape)
		{
			m_paused = !m_paused;
			GetContext().music->SetPaused(m_paused);
		}
		// If backspace is pressed while paused, exit to main menu
		else if (key_pressed->scancode == sf::Keyboard::Scancode::Backspace && m_paused)
		{
			RequestStackClear();
			RequestStackPush(StateID::kMenu);
		}
		// Toggle stats display with 'M' key
		else if (key_pressed->scancode == sf::Keyboard::Scancode::M)
		{
			m_show_stats = !m_show_stats;
		}
	}
	else if (event.is<sf::Event::FocusGained>())
	{
		m_has_focus = true;
	}
	else if (event.is<sf::Event::FocusLost>())
	{
		m_has_focus = false;
	}
	return true;
}

void MultiplayerGameState::OnActivate()
{
	m_active_state = true;
}

void MultiplayerGameState::OnDestroy()
{
	if (!m_host && m_connected)
	{
		//Inform server this client is dying
		sf::Packet packet;
		packet << static_cast<uint8_t>(Client::PacketType::kQuit);
		m_socket.send(packet);
	}
}

void MultiplayerGameState::DisableAllRealtimeActions(bool enable)
{
	m_active_state = enable;
	for (uint8_t identifier : m_local_player_identifiers)
	{
		m_players[identifier]->DisableAllRealtimeActions(enable);
	}
}

void MultiplayerGameState::UpdateBroadcastMessage(sf::Time elapsed_time)
{
	if (m_broadcasts.empty())
	{
		return;
	}

	//Update broadcast timer
	m_broadcast_elapsed_time += elapsed_time;
	if (m_broadcast_elapsed_time > sf::seconds(2.f))
	{
		//If message has expired, remove it
		m_broadcasts.erase(m_broadcasts.begin());

		//Continue to display the next broadcast message
		if (!m_broadcasts.empty())
		{
			m_broadcast_text.setString(m_broadcasts.front());
			Utility::CentreOrigin(m_broadcast_text);
			m_broadcast_elapsed_time = sf::Time::Zero;
		}
	}
}

void MultiplayerGameState::HandlePacket(uint8_t packet_type, sf::Packet& packet)
{
	switch (static_cast<Server::PacketType>(packet_type))
	{
		//Send message to all Clients
	case Server::PacketType::kBroadcastMessage:
	{
		std::string message;
		packet >> message;
		m_broadcasts.push_back(message);

		//Just added the first message, display immediately
		if (m_broadcasts.size() == 1)
		{
			m_broadcast_text.setString(m_broadcasts.front());
			Utility::CentreOrigin(m_broadcast_text);
			m_broadcast_elapsed_time = sf::Time::Zero;
		}
	}
	break;

	//Sent by the server to spawn player 1 airplane on connect
	case Server::PacketType::kSpawnSelf:
	{
		uint8_t aircraft_identifier;
		sf::Vector2f aircraft_position;
		packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y;

		// Convert aircraft_identifier (1-based) to PlayerID (0-based)
		PlayerID player_id = static_cast<PlayerID>(aircraft_identifier - 1);
		Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier, player_id);
		if (aircraft)
		{
			aircraft->setPosition(aircraft_position);
			// This is the local player's own aircraft: enable client-side
			// prediction + reconciliation for it.
			aircraft->SetLocallyControlled(true);
		}

		// Use player_id (0-based) as the Player's identifier, not aircraft_identifier (1-based)
		m_players[aircraft_identifier].reset(new Player(&m_socket, static_cast<uint8_t>(player_id), GetContext().keys1, aircraft_identifier));
		m_local_player_identifiers.push_back(aircraft_identifier);
		m_game_started = true;
	}
	break;

	case Server::PacketType::kSpawnPickup:
	{
		uint32_t pickup_count;
		packet >> pickup_count;  // Read number of pickups in this batch

		for (uint32_t i = 0; i < pickup_count; ++i)
		{
			uint32_t pickup_id;
			int32_t pickup_type;
			float x, y;
			packet >> pickup_id >> pickup_type >> x >> y;
			m_world.SpawnPickupFromNetwork(pickup_id, pickup_type, sf::Vector2f(x, y));
		}

	}
	break;

	case Server::PacketType::kPickupCollected:
	{
		uint32_t despawn_count;
		packet >> despawn_count;

		for (uint32_t i = 0; i < despawn_count; ++i)
		{
			uint32_t pickup_id;
			packet >> pickup_id;
			m_world.RemovePickup(pickup_id);
		}
	}
	break;

	case Server::PacketType::kPlayerConnect:
	{
		uint8_t aircraft_identifier;
		sf::Vector2f aircraft_position;
		packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y;

		// Convert aircraft_identifier (1-based) to PlayerID (0-based)
		PlayerID player_id = static_cast<PlayerID>(aircraft_identifier - 1);
		Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier, player_id);
		if (aircraft)
		{
			aircraft->setPosition(aircraft_position);
		}

		// Use player_id (0-based) as the Player's identifier, not aircraft_identifier (1-based)
		m_players[aircraft_identifier].reset(new Player(&m_socket, static_cast<uint8_t>(player_id), nullptr, aircraft_identifier));
	}
	break;

	case Server::PacketType::kPlayerDisconnect:
	{
		uint8_t aircraft_identifier;
		packet >> aircraft_identifier;

		// Unregister the player from the gameplay manager to destroy their GUI
		GameplayManager* gm = m_world.GetGameplayManager();
		if (gm)
		{
			// Registration uses a 0-based player index, aircraft_identifier is 1-based
			gm->UnregisterPlayer(aircraft_identifier - 1);
		}

		m_world.RemoveAircraft(aircraft_identifier);
		m_players.erase(aircraft_identifier);
	}
	break;

	case Server::PacketType::kInitialState:
	{
		uint8_t aircraft_count;
		packet >> aircraft_count;

		for (uint8_t i = 0; i < aircraft_count; ++i)
		{
			uint8_t aircraft_identifier;
			uint8_t hitpoints;
			uint8_t missile_ammo;
			sf::Vector2f aircraft_position;
			packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> hitpoints >> missile_ammo;

			// Convert aircraft_identifier (1-based) to PlayerID (0-based)
			PlayerID player_id = static_cast<PlayerID>(aircraft_identifier - 1);
			Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier, player_id);
			if (aircraft)
			{
				aircraft->setPosition(aircraft_position);
				aircraft->SetHitpoints(hitpoints);
				aircraft->GetWeaponSystem().SetMissileAmmo(missile_ammo);
			}

			m_players[aircraft_identifier].reset(new Player(&m_socket, static_cast<uint8_t>(player_id), nullptr, aircraft_identifier));
		}
	}
	break;

	//Player event, like missile fired occurs
	case Server::PacketType::kPlayerEvent:
	{
		uint8_t aircraft_identifier;
		uint8_t action;
		packet >> aircraft_identifier >> action;

		auto itr = m_players.find(aircraft_identifier);
		if (itr != m_players.end())
		{
			itr->second->HandleNetworkEvent(static_cast<Action>(action)
				, m_world.GetCommandQueue());
		}
	}
	break;

	//Player's movement or fire keyboard state changes
	case Server::PacketType::kPlayerRealtimeChange:
	{
		uint8_t aircraft_identifier;
		uint8_t action;
		bool action_enabled;
		packet >> aircraft_identifier >> action >> action_enabled;

		auto itr = m_players.find(aircraft_identifier);
		if (itr != m_players.end())
		{
			itr->second->HandleNetworkRealtimeChange(static_cast<Action>(action), action_enabled);
		}
	}
	break;


	//Should tell what player has won and how many points
	case Server::PacketType::kMissionSuccess:
	{
		if (GameplayManager* gameplay_manager = m_world.GetGameplayManager())
		{
			GetContext().player->SetGameplayManager(gameplay_manager);
		}
		m_mission_over = true;
		RequestStackPush(StateID::kGameOver);
	}
	break;

	case Server::PacketType::kUpdateClientState:
	{
		uint8_t aircraft_count;
		packet >> aircraft_count;

		for (uint8_t i = 0; i < aircraft_count; ++i)
		{
			sf::Vector2f aircraft_position;
			float rotation;
			uint8_t aircraft_identifier;
			packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> rotation;

			Aircraft* aircraft = m_world.GetAircraft(aircraft_identifier);

			if (aircraft)
			{
				if (aircraft->IsLocallyControlled())
				{
					// Local player's own aircraft: feed the authoritative state
					// into the reconciliation path so client-side prediction is
					// gently corrected instead of overwritten.
					aircraft->ApplyServerCorrection(aircraft_position, rotation);
				}
				else
				{
					// Remote aircraft: buffer the state for smooth interpolation.
					aircraft->AddNetworkSnapshot(aircraft_position, rotation);
				}
			}
		}
	}
	break;
	}
}