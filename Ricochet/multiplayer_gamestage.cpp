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

sf::IpAddress GetAddressFromFile()
{
	{
		//Try to open existing file
		std::ifstream input_file("ip.txt");
		std::string ip_address;
		if (input_file >> ip_address)
		{
			if (auto address = sf::IpAddress::resolve(ip_address))
			{
				return *address;
			}
		}
	}

	//If the open/read failed or IP was invalid, create/overwrite the file with correct IP
	std::ofstream output_file("ip.txt");
	sf::IpAddress local_address = sf::IpAddress::LocalHost;
	output_file << local_address.toString();
	output_file.close();
	return local_address;
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
{
	m_broadcast_text.setPosition(sf::Vector2f(1024.f / 2, 100.f));

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
		std::cout << "[CLIENT] Attempting connection to " << ip->toString() << ":" << SERVER_PORT << " - Status: " << static_cast<int>(status) << std::endl;

		if (status == sf::Socket::Status::Done)
		{
			m_connected = true;
			std::cout << "[CLIENT] Connection SUCCESSFUL" << std::endl;
			// Set non-blocking ONLY after a successful connection
			m_socket.setBlocking(false);
		}
		else
		{
			std::cout << "[CLIENT] Connection FAILED - will return to menu after 5 seconds" << std::endl;
			m_failed_connection_clock.restart();
		}
	}
	else
	{
		m_failed_connection_clock.restart();
	}

	//Set socket to non-blocking
	m_socket.setBlocking(false);

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
		//Handle all realtime input for players with correct PlayerID
		//In multiplayer, we DON'T execute commands on aircraft - the server handles all movement
		//We only send the input to the server and apply server-broadcast positions
		CommandQueue& commands = m_world.GetCommandQueue();
		if (m_active_state && m_has_focus)
		{
			for (auto& pair : m_players)
			{
				pair.second->HandleRealTimeInput(commands);
			}
		}

		// Clear the command queue - we don't execute movement commands on networked aircraft
		// The server will simulate movement and send us the positions
		while (!commands.IsEmpty())
		{
			commands.Pop();
		}

		m_world.Update(dt);

		//Remove players whose aircraft were destroyed
		for (auto itr = m_players.begin(); itr != m_players.end();)
		{
			if (!m_world.GetAircraft(itr->first))
			{
				itr = m_players.erase(itr);

				//No more players left : Mission failed
				if (m_players.empty())
				{
					RequestStackPush(StateID::kGameOver);
				}
			}
			else
			{
				++itr;
			}
		}

		//Handle messages from the server that may have arrived
		sf::Packet packet;
		if (m_socket.receive(packet) == sf::Socket::Status::Done)
		{
			m_time_since_last_packet = sf::seconds(0.f);
			uint8_t packet_type;
			packet >> packet_type;
			HandlePacket(packet_type, packet);
		}
		else
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
		}

		//Send input commands for all local aircraft
		for (auto& pair : m_players)
		{
			uint8_t aircraft_identifier = pair.first;
			Player* player = pair.second.get();

			// Send all active/inactive actions for this aircraft
			// GetActionProxies contains the current state of all actions
			const auto& action_proxies = player->GetActionProxies();
			for (const auto& action_pair : action_proxies)
			{
				sf::Packet input_packet;
				input_packet << static_cast<uint8_t>(Client::PacketType::kInputCommand);
				input_packet << aircraft_identifier;
				input_packet << static_cast<uint8_t>(action_pair.first);
				input_packet << action_pair.second;

				m_socket.send(input_packet);
			}
		}
		m_time_since_last_packet = sf::seconds(0.f);
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
		//If escape is pressed, show the pause screen
		if (key_pressed->scancode == sf::Keyboard::Scancode::Escape)
		{
			DisableAllRealtimeActions(false);
			RequestStackPush(StateID::kNetworkPause);
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
	for (auto& pair : m_players)
	{
		pair.second->DisableAllRealtimeActions(enable);
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


		Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier);
		if (aircraft)
		{
			aircraft->setPosition(aircraft_position);
		}
		else
		{
			std::cout << "[MULTIPLAYER] ERROR: Failed to add aircraft to world!" << std::endl;
		}

		m_players[aircraft_identifier].reset(new Player(&m_socket, aircraft_identifier, GetContext().keys1));
		m_game_started = true;
	}
	break;

	case Server::PacketType::kPlayerConnect:
	{
		uint8_t aircraft_identifier;
		sf::Vector2f aircraft_position;
		packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y;


		Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier);
		if (aircraft)
		{
			std::cout << "[MULTIPLAYER] *** SUCCESSFULLY ADDED REMOTE AIRCRAFT TO WORLD ***" << std::endl;
			aircraft->setPosition(aircraft_position);
		}
		else
		{
			std::cout << "[MULTIPLAYER] ERROR: Failed to add remote aircraft to world!" << std::endl;
		}

		m_players[aircraft_identifier].reset(new Player(&m_socket, aircraft_identifier, nullptr));
	}
	break;

	case Server::PacketType::kPlayerDisconnect:
	{
		uint8_t aircraft_identifier;
		packet >> aircraft_identifier;
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


			Aircraft* aircraft = m_world.AddAircraft(aircraft_identifier);
			if (aircraft)
			{
				aircraft->setPosition(aircraft_position);
				aircraft->SetHitpoints(hitpoints);
				aircraft->GetWeaponSystem().SetMissileAmmo(missile_ammo);
			}
			else
			{
				std::cout << "[MULTIPLAYER] ERROR: Failed to add aircraft ID " << static_cast<int>(aircraft_identifier) << std::endl;
			}

			m_players[aircraft_identifier].reset(new Player(&m_socket, aircraft_identifier, nullptr));
		}
	}
	break;

	case Server::PacketType::kAcceptCoopPartner:
	{
		uint8_t aircraft_identifier;
		packet >> aircraft_identifier;

		m_world.AddAircraft(aircraft_identifier);
		m_players[aircraft_identifier].reset(new Player(&m_socket, aircraft_identifier, GetContext().keys1));
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
		RequestStackPush(StateID::kGameOver);
	}
	break;

	//Pickup created
	case Server::PacketType::kSpawnPickup:
	{
		uint8_t type;
		sf::Vector2f position;
		packet >> type >> position.x >> position.y;
		m_world.SpawnRandomPickups();
	}
	break;

	case Server::PacketType::kUpdateClientState:
	{
		uint8_t aircraft_count;
		packet >> aircraft_count;

		for (uint8_t i = 0; i < aircraft_count; ++i)
		{
			sf::Vector2f aircraft_position;
			float aircraft_rotation;
			uint8_t aircraft_identifier;
			uint8_t hitpoints;
			uint8_t ammo;
			packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> aircraft_rotation >> hitpoints >> ammo;

			Aircraft* aircraft = m_world.GetAircraft(aircraft_identifier);

			if (aircraft)
			{
				// Apply server positions uniformly to all aircraft
				// Light interpolation to smooth movement
				sf::Vector2f current_pos = aircraft->getPosition();
				sf::Vector2f interpolated_position = current_pos + (aircraft_position - current_pos) * 0.5f;
				aircraft->setPosition(interpolated_position);

				// Apply server rotation directly (no interpolation needed for rotation)
				aircraft->setRotation(sf::degrees(aircraft_rotation));

				aircraft->SetHitpoints(hitpoints);
				aircraft->GetWeaponSystem().SetMissileAmmo(ammo);
			}
		}
	}
	break;
	}
}