#include "game_server.hpp"
#include "network_protocol.hpp"
#include "aircraft_type.hpp"
#include "pickup_type.hpp"
#include "utility.hpp"
#include "physics_simulator.hpp"
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>
#include <iostream>
#include <cmath>

GameServer::GameServer(sf::Vector2f battlefield_size)
    : m_listening_state(false)
    , m_client_timeout(sf::seconds(5.f))
    , m_max_connected_players(20)
    , m_connected_players(0)
    , m_battlefield_rect(
        sf::Vector2f(0.f, 0.f),
        battlefield_size
    )
    , m_aircraft_count(0)
    , m_peers(m_max_connected_players)
    , m_aircraft_identifier_counter(1)
    , m_last_spawn_time(sf::Time::Zero)
    , m_time_for_next_spawn(sf::seconds(5.f))
{
    m_listener_socket.setBlocking(false);

    for (std::size_t i = 0; i < m_max_connected_players; ++i)
    {
        m_peers[i] = std::make_unique<RemotePeer>();
    }

    SetListening(true);

    if (!m_listening_state)
    {
        return;
    }

    m_thread = std::thread(&GameServer::ExecutionThread, this);
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
        }
    }
    else
    {
        m_listener_socket.close();
        m_listening_state = false;
    }
}

void GameServer::ExecutionThread()
{
    try
    {

        //Initialisation
        //SetListening(true);

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
    // Apply server-side physics: bounce aircraft off walls
    if (m_physics_simulator)
    {
        for (auto& aircraft_pair : m_aircraft_info)
        {
            AircraftInfo& info = aircraft_pair.second;
            sf::FloatRect aircraft_bounds(info.m_position, sf::Vector2f(40.f, 40.f));  // Approximate aircraft size

            // Check left boundary
            if (aircraft_bounds.position.x <= m_battlefield_rect.position.x)
            {
                info.m_position.x = m_battlefield_rect.position.x + (aircraft_bounds.size.x / 2.f);
            }
            // Check right boundary
            else if (aircraft_bounds.position.x + aircraft_bounds.size.x >= m_battlefield_rect.position.x + m_battlefield_rect.size.x)
            {
                info.m_position.x = m_battlefield_rect.position.x + m_battlefield_rect.size.x - (aircraft_bounds.size.x / 2.f);
            }

            // Check top boundary
            if (aircraft_bounds.position.y <= m_battlefield_rect.position.y)
            {
                info.m_position.y = m_battlefield_rect.position.y + (aircraft_bounds.size.y / 2.f);
            }
            // Check bottom boundary
            else if (aircraft_bounds.position.y + aircraft_bounds.size.y >= m_battlefield_rect.position.y + m_battlefield_rect.size.y)
            {
                info.m_position.y = m_battlefield_rect.position.y + m_battlefield_rect.size.y - (aircraft_bounds.size.y / 2.f);
            }
        }
    }

    // Now broadcast the corrected positions to all clients
    UpdateClientState();

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

    case Client::PacketType::kStateUpdate:
    {
        uint8_t num_aircraft;
        packet >> num_aircraft;

        for (uint8_t i = 0; i < num_aircraft; ++i)
        {
            uint8_t aircraft_identifier;
            uint8_t aircraft_hitpoints;
            uint8_t missile_ammo;
            sf::Vector2f aircraft_position;
            packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> aircraft_hitpoints >> missile_ammo;
            m_aircraft_info[aircraft_identifier].m_position = aircraft_position;
            m_aircraft_info[aircraft_identifier].m_hitpoints = aircraft_hitpoints;
            m_aircraft_info[aircraft_identifier].m_missile_ammo = missile_ammo;
        }
    }
    break;
    case Client::PacketType::kGameEvent:
    {
        uint8_t action;
        float x;
        float y;

        packet >> action;
        packet >> x;
        packet >> y;

        //Enemy explodes, with a certain probability, drop a pickup
        //To avoid multiple messages only listen to the first peer (host)
        if (action == GameActions::kEnemyExplode && Utility::RandomInt(3) == 0 && &receiving_peer == m_peers[0].get())
        {
            sf::Packet packet;
            packet << static_cast<uint8_t>(Server::PacketType::kSpawnPickup);
            packet << static_cast<uint8_t>(Utility::RandomInt(static_cast<int>(PickupType::kPickupCount)));
            packet << x;
            packet << y;

            SendToAll(packet);
        }
    }
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
        return;
    }

    if (m_listener_socket.accept(m_peers[m_connected_players]->m_socket) == sf::TcpListener::Status::Done)
    {

        //Order the new client to spawn its player 1
        // Vary spawn positions based on aircraft identifier to avoid players spawning on top of each other
        float spawn_offset_angle = (m_aircraft_identifier_counter - 1) * (3.14159f * 2.f / 4.f); // Distribute around center
        float spawn_distance = 150.f;
        sf::Vector2f center(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[m_aircraft_identifier_counter].m_position = center + sf::Vector2f(
            std::cos(spawn_offset_angle) * spawn_distance,
            std::sin(spawn_offset_angle) * spawn_distance
        );
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
        update_client_state_packet << aircraft.first << aircraft.second.m_position.x << aircraft.second.m_position.y << aircraft.second.m_hitpoints << aircraft.second.m_missile_ammo;
    }

    SendToAll(update_client_state_packet);
}

//It is essential to set the sockets to non-blocking - m_socket.setBlocking(false)
//otherwise the server will hang waiting to read input from a connection
GameServer::RemotePeer::RemotePeer()
    : m_ready(false)
    , m_timed_out(false)
{
    m_socket.setBlocking(false);
}
