#include "game_server.hpp"
#include "network_protocol.hpp"
#include "aircraft_type.hpp"
#include "pickup_type.hpp"
#include "action.hpp"
#include "constants.hpp"
#include "utility.hpp"
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>
#include <iostream>
#include <cmath>

GameServer::GameServer(sf::Vector2f battlefield_size)
    : m_listening_state(false)
    , m_client_timeout(sf::seconds(5.f))
    , m_max_connected_players(20)
    , m_connected_players(0)
    , m_battlefield_rect(sf::Vector2f(0.f, 0.f), sf::Vector2f(battlefield_size.x, battlefield_size.y))
    , m_aircraft_count(0)
    , m_peers(m_max_connected_players)
    , m_aircraft_identifier_counter(1)
    , m_waiting_thread_end(false)
    , m_last_spawn_time(sf::Time::Zero)
    , m_time_for_next_spawn(sf::seconds(5.f))
    , m_thread(&GameServer::ExecutionThread, this)
{
    m_listener_socket.setBlocking(false);
    // Pre-allocate all peer slots to prevent out-of-bounds access
    for (std::size_t i = 0; i < m_max_connected_players; ++i)
    {
        m_peers[i].reset(new RemotePeer);
    }
}

GameServer::~GameServer()
{
    m_waiting_thread_end = true;
    m_thread.join();
}

void GameServer::NotifyPlayerSpawn(uint8_t aircraft_identifier)
{
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
    packet << aircraft_identifier << m_aircraft_info[aircraft_identifier].m_position.x << m_aircraft_info[aircraft_identifier].m_position.y;
    SendToAll(packet);
}

void GameServer::NotifyPlayerRealtimeChange(uint8_t aircraft_identifier, uint8_t action, bool action_enabled)
{
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerRealtimeChange);
    packet << aircraft_identifier;
    packet << action;
    packet << action_enabled;
    SendToAll(packet);

}

void GameServer::NotifyPlayerEvent(uint8_t aircraft_identifier, int8_t action)
{
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerEvent);
    packet << aircraft_identifier;
    packet << action;
    SendToAll(packet);
}

void GameServer::SetListening(bool enable)
{
    //Check if the server is already listening
    if (enable)
    {
        if (!m_listening_state)
        {
            m_listening_state = (m_listener_socket.listen(SERVER_PORT) == sf::TcpListener::Status::Done);
            std::cout << "[GAMESERVER] SetListening(true) - Listening on port " << SERVER_PORT 
                      << " - State: " << (m_listening_state ? "SUCCESS" : "FAILED") << std::endl;
        }
    }
    else
    {
        m_listener_socket.close();
        m_listening_state = false;
        std::cout << "[GAMESERVER] SetListening(false) - Listening stopped" << std::endl;
    }
}

void GameServer::ExecutionThread()
{
    try
    {
        std::cout << "[GAMESERVER] ExecutionThread started" << std::endl;

        //Initialisation
        SetListening(true);

        sf::Time frame_rate = sf::seconds(1.f / 60.f);
        sf::Time frame_time = sf::Time::Zero;
        sf::Time tick_rate = sf::seconds(1.f / 20.f);
        sf::Time tick_time = sf::Time::Zero;
        sf::Clock frame_clock, tick_clock;

        while (!m_waiting_thread_end)
        {
            //This is the game loop
            HandleIncomingConnections();
            HandleIncomingPackets();

            frame_time += frame_clock.getElapsedTime();
            frame_clock.restart();
            tick_time += tick_clock.getElapsedTime();
            tick_clock.restart();

            //Fixed time step
            while (frame_time >= frame_rate)
            {
                frame_time -= frame_rate;
            }

            while (tick_time >= tick_rate)
            {
                Tick();
                tick_time -= tick_rate;
            }

            //sleep to allow me to run the client on this machine as well
            //maybe rethink this if performance is poor
            sf::sleep(sf::milliseconds(50));
        }
    }
    catch (const std::exception& e)
    {
    }
    catch (...)
    {
    }
}

void GameServer::Tick()
{
    // Simulate aircraft movement based on stored action states
    SimulateMovement(sf::seconds(kTimePerFrame));

    UpdateClientState();

    //Check if the game is over = all planes position.y < offset
    bool all_aircraft_done = true;
    for (const auto& current : m_aircraft_info)
    {
        //As long as one player has not crossed the finish line the game is live
        if (current.second.m_position.y > 0.f)
        {
            all_aircraft_done = false;
            break;
        }
    }
    if (all_aircraft_done)
    {
        sf::Packet mission_success_packet;
        mission_success_packet << static_cast<uint8_t>(Server::PacketType::kMissionSuccess);
        SendToAll(mission_success_packet);
    }

    //Remove aircraft that have been destroyed
    for (auto itr = m_aircraft_info.begin(); itr != m_aircraft_info.end();)
    {
        if (itr->second.m_hitpoints <= 0)
        {
            m_aircraft_info.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    //Check if it is time to spawn enemies
    if (Now() >= m_time_for_next_spawn + m_last_spawn_time)
    {
        //Not going to spawn any enemies towards the end of the level
        if (m_battlefield_rect.position.y > 600.f)
        {
            std::size_t enemy_count = 1 + Utility::RandomInt(2);
            float spawn_centre = static_cast<float>(Utility::RandomInt(500) - 250);

            //If there is only one enemy it will spawn in centre
            float plane_distance = 0.f;
            float next_spawn_position = spawn_centre;

            //If there are two enemies they are centred on the spawncentre
            if (enemy_count == 2)
            {
                plane_distance = static_cast<float>(150 + Utility::RandomInt(250));
                next_spawn_position = spawn_centre - plane_distance / 2.f;
            }

            //Send the spawn packets to the clients
            for (std::size_t i = 0; i < enemy_count; ++i)
            {
                sf::Packet packet;
                packet << static_cast<uint8_t>(Server::PacketType::kSpawnEnemy);
                packet << static_cast<uint8_t>(1 + Utility::RandomInt(static_cast<int>(AircraftType::kAircraftCount) - 1));
                packet << m_battlefield_rect.size.y + 500;
                packet << next_spawn_position;

                next_spawn_position += plane_distance / 2.f;
                SendToAll(packet);
            }
            m_last_spawn_time = Now();
            m_time_for_next_spawn = sf::milliseconds(2000 + Utility::RandomInt(6000));
        }
    }
}

sf::Time GameServer::Now() const
{
    return m_clock.getElapsedTime();
}

void GameServer::HandleIncomingPackets()
{
    bool detected_timeout = false;

    for (PeerPtr& peer : m_peers)
    {
        if (peer->m_ready)
        {
            sf::Packet packet;
            while (peer->m_socket.receive(packet) == sf::Socket::Status::Done)
            {
                //Interpret the packet and react to it
                HandleIncomingPackets(packet, *peer, detected_timeout);

                peer->m_last_packet_time = Now();
                packet.clear();
            }

            if (Now() > peer->m_last_packet_time + m_client_timeout)
            {
                peer->m_timed_out = true;
                detected_timeout = true;
            }
        }
    }

    if (detected_timeout)
    {
        HandleDisconnections();
    }
}

void GameServer::HandleIncomingPackets(sf::Packet& packet, RemotePeer& receiving_peer, bool& detected_timeout)
{
    uint8_t packet_type;
    packet >> packet_type;

    switch (static_cast<Client::PacketType>(packet_type))
    {
    case Client::PacketType::kQuit:
    {
        receiving_peer.m_timed_out = true;
        detected_timeout = true;
    }
    break;

    case Client::PacketType::kPlayerEvent:
    {
        uint8_t aircraft_identifier;
        uint8_t action;
        packet >> aircraft_identifier >> action;
        NotifyPlayerEvent(aircraft_identifier, action);
    }
    break;

    case Client::PacketType::kPlayerRealtimeChange:
    {
        uint8_t aircraft_identifier;
        uint8_t action;
        bool action_enabled;
        packet >> aircraft_identifier >> action >> action_enabled;
        NotifyPlayerRealtimeChange(aircraft_identifier, action, action_enabled);
    }
    break;

    case Client::PacketType::kRequestCoopPartner:
    {
        receiving_peer.m_aircraft_identifiers.emplace_back(m_aircraft_identifier_counter);
        m_aircraft_info[m_aircraft_identifier_counter].m_position = sf::Vector2f(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[m_aircraft_identifier_counter].m_velocity = sf::Vector2f(0.f, 0.f);
        m_aircraft_info[m_aircraft_identifier_counter].m_rotation = 0.f;
        m_aircraft_info[m_aircraft_identifier_counter].m_hitpoints = 100;
        m_aircraft_info[m_aircraft_identifier_counter].m_missile_ammo = 2;

        sf::Packet request_packet;
        request_packet << static_cast<uint8_t>(Server::PacketType::kAcceptCoopPartner);
        request_packet << m_aircraft_identifier_counter;
        request_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        request_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        receiving_peer.m_socket.send(request_packet);
        m_aircraft_count++;

        // Tell everyone else about the new plane
        sf::Packet notify_packet;
        notify_packet << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
        notify_packet << m_aircraft_identifier_counter;
        notify_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        notify_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        for (PeerPtr& peer : m_peers)
        {
            if (peer.get() != &receiving_peer && peer->m_ready)
            {
                peer->m_socket.send(notify_packet);
            }
        }
        m_aircraft_identifier_counter++;
    }
    break;

    case Client::PacketType::kInputCommand:
    {
        uint8_t aircraft_identifier;
        uint8_t action_type;
        bool action_enabled;
        packet >> aircraft_identifier >> action_type >> action_enabled;

        // Store the action state for this aircraft
        // This will be used by the server's simulation to move the aircraft
        if (m_aircraft_info.find(aircraft_identifier) != m_aircraft_info.end())
        {
            m_aircraft_info[aircraft_identifier].m_real_time_actions[action_type] = action_enabled;
        }
    }
    break;

    case Client::PacketType::kGameEvent:
    {
        uint8_t action;
        float x;
        // TODO: Handle game event
    }
    break;
    }
}

void GameServer::HandleIncomingConnections()
{
    if (!m_listening_state)
    {
        return;
    }

    // Defensive bounds check to prevent out-of-bounds access
    if (m_connected_players >= m_peers.size())
    {
        std::cout << "[GAMESERVER] WARNING: m_connected_players (" << m_connected_players 
                  << ") exceeds m_peers size (" << m_peers.size() << "). Cannot accept more connections." << std::endl;
        return;
    }

    if (m_listener_socket.accept(m_peers[m_connected_players]->m_socket) == sf::TcpListener::Status::Done)
    {
        std::cout << "[GAMESERVER] Player attempting to join! Total connected: " << static_cast<int>(m_connected_players + 1) << std::endl;

        //Order the new client to spawn its player 1
        m_aircraft_info[m_aircraft_identifier_counter].m_position = sf::Vector2f(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[m_aircraft_identifier_counter].m_velocity = sf::Vector2f(0.f, 0.f);
        m_aircraft_info[m_aircraft_identifier_counter].m_rotation = 0.f;
        m_aircraft_info[m_aircraft_identifier_counter].m_hitpoints = 100;
        m_aircraft_info[m_aircraft_identifier_counter].m_missile_ammo = 2;

        sf::Packet packet;
        packet << static_cast<uint8_t>(Server::PacketType::kSpawnSelf);
        packet << m_aircraft_identifier_counter;
        packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        m_peers[m_connected_players]->m_aircraft_identifiers.emplace_back(m_aircraft_identifier_counter);

        BroadcastMessage("New player");
        InformWorldState(m_peers[m_connected_players]->m_socket);
        NotifyPlayerSpawn(m_aircraft_identifier_counter++);

        m_peers[m_connected_players]->m_socket.send(packet);
        m_peers[m_connected_players]->m_ready = true;
        m_peers[m_connected_players]->m_last_packet_time = Now();

        m_aircraft_count++;
        m_connected_players++;

        if (m_connected_players >= m_max_connected_players)
        {
            SetListening(false);
        }
    }
    else
    {
        // This is normal - means no connections are waiting, not an error
        // Commenting out to reduce noise in logs
        // std::cout << "[GAMESERVER] No pending connections at this moment" << std::endl;
    }
}

void GameServer::HandleDisconnections()
{
    for (auto itr = m_peers.begin(); itr != m_peers.end();)
    {
        if ((*itr)->m_timed_out)
        {
            //Inform everyone of a disconnection, erase
            for (uint8_t identifer : (*itr)->m_aircraft_identifiers)
            {
                SendToAll((sf::Packet() << static_cast<uint8_t>(Server::PacketType::kPlayerDisconnect) << identifer));
                m_aircraft_info.erase(identifer);
            }

            m_connected_players--;
            m_aircraft_count -= (*itr)->m_aircraft_identifiers.size();

            itr = m_peers.erase(itr);

            //If the number of peers has dropped below max_connections
            if (m_connected_players < m_max_connected_players)
            {
                m_peers.emplace_back(PeerPtr(new RemotePeer()));
                SetListening(true);
            }

            BroadcastMessage("A player has disconnected");

        }
        else
        {
            ++itr;
        }
    }
}

void GameServer::InformWorldState(sf::TcpSocket& socket)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kInitialState);
    packet << static_cast<uint8_t>(m_aircraft_count);

    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            for (uint8_t identifier : m_peers[i]->m_aircraft_identifiers)
            {
                packet << identifier << m_aircraft_info[identifier].m_position.x << m_aircraft_info[identifier].m_position.y << m_aircraft_info[identifier].m_hitpoints << m_aircraft_info[identifier].m_missile_ammo;
            }
        }
    }

    socket.send(packet);
}

void GameServer::BroadcastMessage(const std::string& message)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kBroadcastMessage);
    packet << message;
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            m_peers[i]->m_socket.send(packet);
        }
    }
}

void GameServer::SendToAll(sf::Packet& packet)
{
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            m_peers[i]->m_socket.send(packet);
        }
    }
}

void GameServer::UpdateClientState()
{
    sf::Packet update_client_state_packet;
    update_client_state_packet << static_cast<uint8_t>(Server::PacketType::kUpdateClientState);
    update_client_state_packet << static_cast<uint8_t>(m_aircraft_count);

    for (const auto& aircraft : m_aircraft_info)
    {
        update_client_state_packet << aircraft.first << aircraft.second.m_position.x << aircraft.second.m_position.y << aircraft.second.m_rotation << aircraft.second.m_hitpoints << aircraft.second.m_missile_ammo;
    }

    SendToAll(update_client_state_packet);
}

void GameServer::SimulateMovement(sf::Time dt)
{
    const float kRotationSpeed = 2.5f;  // degrees per frame
    const float kMaxSpeed = 300.f;      // pixels per second
    const float kAccelerationRate = 300.f;
    const float kBoostedAccelerationRate = 15000.f;
    const float kBoostThreshold = 2.f;
    const float kFrictionDamping = 0.95f;  // Velocity multiplier per frame for drag

    for (auto& pair : m_aircraft_info)
    {
        AircraftInfo& aircraft = pair.second;

        // Check if move left action is active
        if (aircraft.m_real_time_actions[static_cast<uint8_t>(Action::kMoveLeft)])
        {
            // Rotate left
            aircraft.m_rotation -= kRotationSpeed;

            // Align velocity to new rotation
            double radians = Utility::toRadians(aircraft.m_rotation + 90.f);
            float speed = std::sqrt(aircraft.m_velocity.x * aircraft.m_velocity.x + 
                                   aircraft.m_velocity.y * aircraft.m_velocity.y);
            if (speed > 0.f)
            {
                aircraft.m_velocity.x = speed * (-std::cos(radians));
                aircraft.m_velocity.y = speed * (-std::sin(radians));
            }
        }

        // Check if move right action is active
        if (aircraft.m_real_time_actions[static_cast<uint8_t>(Action::kMoveRight)])
        {
            // Rotate right
            aircraft.m_rotation += kRotationSpeed;

            // Align velocity to new rotation
            double radians = Utility::toRadians(aircraft.m_rotation + 90.f);
            float speed = std::sqrt(aircraft.m_velocity.x * aircraft.m_velocity.x + 
                                   aircraft.m_velocity.y * aircraft.m_velocity.y);
            if (speed > 0.f)
            {
                aircraft.m_velocity.x = speed * (-std::cos(radians));
                aircraft.m_velocity.y = speed * (-std::sin(radians));
            }
        }

        // Check if move up (forward) action is active
        if (aircraft.m_real_time_actions[static_cast<uint8_t>(Action::kMoveUp)])
        {
            // Accelerate forward in the direction of rotation
            double radians = Utility::toRadians(aircraft.m_rotation + 90.f);
            float dirX = -std::cos(radians);
            float dirY = -std::sin(radians);

            float currentSpeed = std::sqrt(aircraft.m_velocity.x * aircraft.m_velocity.x + 
                                         aircraft.m_velocity.y * aircraft.m_velocity.y);

            float acceleration = kAccelerationRate * dt.asSeconds();

            // Apply boost if held for longer than threshold
            // (simplified: just use boost rate for now)
            acceleration = kBoostedAccelerationRate * dt.asSeconds();

            if (currentSpeed < kMaxSpeed)
            {
                float newSpeed = std::min(currentSpeed + acceleration, kMaxSpeed);
                aircraft.m_velocity.x = newSpeed * dirX;
                aircraft.m_velocity.y = newSpeed * dirY;
            }
            else
            {
                aircraft.m_velocity.x = kMaxSpeed * dirX;
                aircraft.m_velocity.y = kMaxSpeed * dirY;
            }
        }
        else
        {
            // Apply friction/drag when not accelerating
            aircraft.m_velocity.x *= kFrictionDamping;
            aircraft.m_velocity.y *= kFrictionDamping;
        }

        // Apply velocity to position
        aircraft.m_position.x += aircraft.m_velocity.x * dt.asSeconds();
        aircraft.m_position.y += aircraft.m_velocity.y * dt.asSeconds();

        // Clamp position to battlefield bounds
        // Bounce off boundaries or stop at edges (simple clamping for now)
        if (aircraft.m_position.x < m_battlefield_rect.position.x)
        {
            aircraft.m_position.x = m_battlefield_rect.position.x;
            aircraft.m_velocity.x = 0.f;  // Stop horizontal movement
        }
        if (aircraft.m_position.x > m_battlefield_rect.position.x + m_battlefield_rect.size.x)
        {
            aircraft.m_position.x = m_battlefield_rect.position.x + m_battlefield_rect.size.x;
            aircraft.m_velocity.x = 0.f;  // Stop horizontal movement
        }
        if (aircraft.m_position.y < m_battlefield_rect.position.y)
        {
            aircraft.m_position.y = m_battlefield_rect.position.y;
            aircraft.m_velocity.y = 0.f;  // Stop vertical movement
        }
        if (aircraft.m_position.y > m_battlefield_rect.position.y + m_battlefield_rect.size.y)
        {
            aircraft.m_position.y = m_battlefield_rect.position.y + m_battlefield_rect.size.y;
            aircraft.m_velocity.y = 0.f;  // Stop vertical movement
        }
    }
}

//It is essential to set the sockets to non-blocking - m_socket.setBlocking(false)
//otherwise the server will hang waiting to read input from a connection
GameServer::RemotePeer::RemotePeer()
    : m_ready(false)
    , m_timed_out(false)
{
    m_socket.setBlocking(false);
}
