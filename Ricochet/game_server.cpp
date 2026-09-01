#include "game_server.hpp"
#include "network_protocol.hpp"
#include "aircraft_type.hpp"
#include "pickup_type.hpp"
#include "action.hpp"
#include "utility.hpp"
#include "physics_simulator.hpp"
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>
#include <iostream>
#include <cmath>

GameServer::GameServer(sf::Vector2f battlefield_size)
    : m_listening_state(false)
    , m_client_timeout(sf::seconds(1.f))
    , m_max_connected_players(15)
    , m_connected_players(0)
    , m_battlefield_rect(
        sf::Vector2f(0.f, 0.f),
        battlefield_size
    )
    , m_aircraft_count(0)
    , m_peers(m_max_connected_players)
    , m_last_spawn_time(sf::Time::Zero)
    , m_time_for_next_spawn(sf::seconds(5.f))
    , m_network_tick_counter(0)
{
    // Initialize all aircraft IDs (1-255) as available for assignment
    for (int id = 1; id < 256; ++id)
    {
        m_available_aircraft_ids.insert(static_cast<uint8_t>(id));
    }

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

void GameServer::NotifyPickupSpawn(uint32_t pickup_id, int pickup_type, sf::Vector2f position)
{
    // Queue the pickup spawn for batching instead of sending immediately
    m_batched_pickup_spawns.push_back({pickup_id, static_cast<int32_t>(pickup_type), position.x, position.y});
}

void GameServer::FlushPickupBatch()
{
    if (m_batched_pickup_spawns.empty())
    {
        return;  // Nothing to send
    }

    // Create a single batch packet containing all pending pickup spawns
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kSpawnPickup);
    packet << static_cast<uint32_t>(m_batched_pickup_spawns.size());  // Number of pickups in this batch

    for (const auto& spawn : m_batched_pickup_spawns)
    {
        packet << spawn.pickup_id << spawn.pickup_type << spawn.x << spawn.y;
    }

    SendToAll(packet);
    m_batched_pickup_spawns.clear();  // Clear the batch after sending
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

    // Send state updates every 2 ticks (10 Hz network, 20 Hz simulation)
    m_network_tick_counter++;
    if (m_network_tick_counter >= 2)
    {
        UpdateClientState();
        FlushPickupBatch();  // Flush any queued pickup spawns
        m_network_tick_counter = 0;
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
            sf::Socket::Status status = peer->m_socket.receive(packet);

            if (status == sf::Socket::Status::Disconnected)
            {
                // Socket closed immediately on receive failure
                peer->m_timed_out = true;
                detected_timeout = true;
            }
            else if (status == sf::Socket::Status::Done)
            {
                // Got a packet - process it
                do
                {
                    //Interpret the packet and react to it
                    HandleIncomingPackets(packet, *peer, detected_timeout);

                    {
                        std::lock_guard<std::mutex> lock(m_stats_mutex);
                        m_packets_received++;
                    }

                    peer->m_last_packet_time = Now();
                    packet.clear();
                    status = peer->m_socket.receive(packet);
                } while (status == sf::Socket::Status::Done);

                // After processing packets, check if disconnected
                if (status == sf::Socket::Status::Disconnected)
                {
                    peer->m_timed_out = true;
                    detected_timeout = true;
                }
            }
            else if (status == sf::Socket::Status::NotReady)
            {
                // No data available yet, check timeout only if we haven't received anything in a while
                if (Now() > peer->m_last_packet_time + m_client_timeout)
                {
                    peer->m_timed_out = true;
                    detected_timeout = true;
                }
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

    case Client::PacketType::kInputCommand:
    {
        uint8_t aircraft_identifier;
        uint8_t input_sequence;
        uint8_t input_flags;
        packet >> aircraft_identifier >> input_sequence >> input_flags;

        // Decode bitfield flags and call NotifyPlayerRealtimeChange for each active action
        // Bit 0 = MoveLeft
        if (input_flags & (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveLeft)))
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveLeft), true);
        else
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveLeft), false);

        // Bit 1 = MoveRight
        if (input_flags & (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveRight)))
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveRight), true);
        else
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveRight), false);

        // Bit 2 = MoveUp
        if (input_flags & (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveUp)))
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveUp), true);
        else
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMoveUp), false);

        // Bit 3 = BulletFire
        if (input_flags & (1 << static_cast<uint8_t>(InputCommand::InputFlag::kBulletFire)))
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kBulletFire), true);
        else
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kBulletFire), false);

        // Bit 4 = MissileFire
        if (input_flags & (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMissileFire)))
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMissileFire), true);
        else
            NotifyPlayerRealtimeChange(aircraft_identifier, static_cast<uint8_t>(Action::kMissileFire), false);
    }
    break;

    case Client::PacketType::kStateUpdate:
    {
        uint8_t num_aircraft;
        packet >> num_aircraft;

        for (uint8_t i = 0; i < num_aircraft; ++i)
        {
            uint8_t aircraft_identifier;
            sf::Vector2f aircraft_position;
            float rotation;
            packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> rotation;

            m_aircraft_info[aircraft_identifier].m_position = aircraft_position;
            m_aircraft_info[aircraft_identifier].m_rotation = rotation;
        }
    }
    break;

    case Client::PacketType::kHeartbeat:
    {
        // Heartbeat received - just reset the timeout timer (handled by receiving packet itself)
        // No action needed, just acknowledgment through packet reception
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

    // Check if there are any available IDs
    if (m_available_aircraft_ids.empty())
    {
        return;
    }

    if (m_listener_socket.accept(m_peers[m_connected_players]->m_socket) == sf::TcpListener::Status::Done)
    {
        // Get the lowest available aircraft ID
        uint8_t aircraft_id = *m_available_aircraft_ids.begin();
        m_available_aircraft_ids.erase(m_available_aircraft_ids.begin());

        //Order the new client to spawn its player 1
        // Vary spawn positions based on aircraft identifier to avoid players spawning on top of each other
        float spawn_offset_angle = (aircraft_id - 1) * (3.14159f * 2.f / 4.f); // Distribute around center
        float spawn_distance = 150.f;
        sf::Vector2f center(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[aircraft_id].m_position = center + sf::Vector2f(
            std::cos(spawn_offset_angle) * spawn_distance,
            std::sin(spawn_offset_angle) * spawn_distance
        );
        m_aircraft_info[aircraft_id].m_hitpoints = 100;
        m_aircraft_info[aircraft_id].m_missile_ammo = 2;

        sf::Packet packet;
        packet << static_cast<uint8_t>(Server::PacketType::kSpawnSelf);
        packet << aircraft_id;
        packet << m_aircraft_info[aircraft_id].m_position.x;
        packet << m_aircraft_info[aircraft_id].m_position.y;

        m_peers[m_connected_players]->m_aircraft_identifiers.emplace_back(aircraft_id);

        BroadcastMessage("New player");
        InformWorldState(m_peers[m_connected_players]->m_socket);
        NotifyPlayerSpawn(aircraft_id);

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

                // Return the ID to the pool of available IDs
                m_available_aircraft_ids.insert(identifer);

                // Track this for host-side GUI cleanup
                m_recently_disconnected_aircraft.push_back(identifer);
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
            {
                std::lock_guard<std::mutex> lock(m_stats_mutex);
                m_packets_sent++;
            }
        }
    }
}

void GameServer::SendToPeer(RemotePeer& peer, sf::Packet& packet)
{
    if (peer.m_ready)
    {
        peer.m_socket.send(packet);
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_packets_sent++;
        }
    }
}

void GameServer::UpdateClientState()
{
    // Send position/rotation updates, per-peer, skipping each client's own aircraft
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (!m_peers[i] || !m_peers[i]->m_ready)
            continue;

        sf::Packet packet;
        packet << static_cast<uint8_t>(Server::PacketType::kUpdateClientState);

        // First pass: count aircraft that are NOT owned by this peer
        uint8_t aircraftCount = 0;
        for (const auto& [id, aircraft] : m_aircraft_info)
        {
            if (std::find(
                    m_peers[i]->m_aircraft_identifiers.begin(),
                    m_peers[i]->m_aircraft_identifiers.end(),
                    id
                ) == m_peers[i]->m_aircraft_identifiers.end())
            {
                ++aircraftCount;
            }
        }

        packet << aircraftCount;

        // Second pass: serialize aircraft data for all aircraft NOT owned by this peer
        for (const auto& [id, aircraft] : m_aircraft_info)
        {
            if (std::find(
                    m_peers[i]->m_aircraft_identifiers.begin(),
                    m_peers[i]->m_aircraft_identifiers.end(),
                    id
                ) == m_peers[i]->m_aircraft_identifiers.end())
            {
                packet
                    << id
                    << aircraft.m_position.x
                    << aircraft.m_position.y
                    << aircraft.m_rotation;
            }
        }

        SendToPeer(*m_peers[i], packet);
    }
}

NetworkStats GameServer::GetNetworkStats() const
{
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    NetworkStats stats;
    stats.packets_sent = m_packets_sent;
    stats.packets_received = m_packets_received;
    stats.connected_players = m_connected_players;
    return stats;
}

std::vector<uint8_t> GameServer::GetAndClearRecentlyDisconnectedAircraft()
{
    std::vector<uint8_t> result = m_recently_disconnected_aircraft;
    m_recently_disconnected_aircraft.clear();
    return result;
}



//It is essential to set the sockets to non-blocking - m_socket.setBlocking(false)
//otherwise the server will hang waiting to read input from a connection
GameServer::RemotePeer::RemotePeer()
    : m_ready(false)
    , m_timed_out(false)
{
    m_socket.setBlocking(false);
}
