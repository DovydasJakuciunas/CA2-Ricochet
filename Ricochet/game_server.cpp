#include "game_server.hpp"
#include "network_protocol.hpp"
#include "aircraft_type.hpp"
#include "pickup_type.hpp"
#include "action.hpp"
#include "utility.hpp"
#include "constants.hpp"
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

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
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
    packet << aircraft_identifier << m_aircraft_info[aircraft_identifier].m_position.x << m_aircraft_info[aircraft_identifier].m_position.y;
    SendToAll(packet);
}

void GameServer::NotifyPlayerRealtimeChange(uint8_t aircraft_identifier, uint8_t action, bool action_enabled)
{
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    // Store the input state so the server can authoritatively simulate movement
    auto found = m_aircraft_info.find(aircraft_identifier);
    if (found != m_aircraft_info.end())
    {
        found->second.m_real_time_actions[action] = action_enabled;
    }

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
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerEvent);
    packet << aircraft_identifier;
    packet << action;
    SendToAll(packet);
}

void GameServer::NotifyPickupSpawn(uint32_t pickup_id, int pickup_type, sf::Vector2f position)
{
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    // Queue the pickup spawn for batching instead of sending immediately
    m_batched_pickup_spawns.push_back({pickup_id, static_cast<int32_t>(pickup_type), position.x, position.y});
}

void GameServer::NotifyPickupCollected(uint32_t pickup_id)
{
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    // Queue the pickup despawn for batching (will be flushed with spawns)
    m_batched_pickup_despawns.push_back(pickup_id);
}

void GameServer::NotifyMissionSuccess()
{
    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
    sf::Packet packet;
    //First thing in every packet is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kMissionSuccess);
    SendToAll(packet);
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

void GameServer::FlushPickupDespawns()
{
    if (m_batched_pickup_despawns.empty())
    {
        return;  // Nothing to send
    }

    // Create a single batch packet containing all pending pickup despawns
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kPickupCollected);
    packet << static_cast<uint32_t>(m_batched_pickup_despawns.size());  // Number of pickups to despawn

    for (uint32_t pickup_id : m_batched_pickup_despawns)
    {
        packet << pickup_id;
    }

    SendToAll(packet);
    m_batched_pickup_despawns.clear();  // Clear the batch after sending
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
            {
                // Guard every access to shared game state (m_peers, m_aircraft_info,
                // batched pickup buffers, etc.) against concurrent Notify* calls from
                // the main thread. The lock is released before sleeping so the main
                // thread is never blocked while this worker idles.
                std::lock_guard<std::recursive_mutex> lock(m_state_mutex);

                //This is the game loop
                HandleIncomingConnections();
                HandleIncomingPackets();

                frame_time += frame_clock.getElapsedTime();
                frame_clock.restart();
                tick_time += tick_clock.getElapsedTime();
                tick_clock.restart();

                //Fixed time step for movement simulation at 60 Hz
                while (frame_time >= frame_rate)
                {
                    SimulateMovement(frame_rate);
                    frame_time -= frame_rate;
                }

                //Network updates at 20 Hz
                while (tick_time >= tick_rate)
                {
                    Tick();
                    tick_time -= tick_rate;
                }
            }

            //sleep to allow me to run the client on this machine as well
            //maybe rethink this if performance is poor
            sf::sleep(sf::milliseconds(50));
        }
    }
    catch (const std::exception& e)
    {
        std::string error_msg = std::string("GameServer thread error: ") + e.what();
        // Log to your logging system (spdlog, custom logger, etc.)
        // your_logger->error(error_msg);
        std::cerr << error_msg << std::endl;
        m_waiting_thread_end = true;
    }
    catch (...)
    {
        std::cerr << "GameServer thread: Unknown exception" << std::endl;
        m_waiting_thread_end = true;
    }
}

void GameServer::SimulateMovement(sf::Time dt)
{
    const float dt_seconds = dt.asSeconds();

    // Movement constants mirror the client model (see constants.hpp / data_tables.cpp)
    const float kAircraftMaxSpeed = 225.f;   // Eagle speed
    const float kMinSpeedThreshold = 0.1f;
    const float kAircraftHalfSize = 24.f;    // 48x48 aircraft

    const float left_bound = m_battlefield_rect.position.x;
    const float right_bound = m_battlefield_rect.position.x + m_battlefield_rect.size.x;
    const float top_bound = m_battlefield_rect.position.y;
    const float bottom_bound = m_battlefield_rect.position.y + m_battlefield_rect.size.y;

    for (auto& aircraft_pair : m_aircraft_info)
    {
        AircraftInfo& info = aircraft_pair.second;

        const bool left = info.m_real_time_actions[static_cast<uint8_t>(Action::kMoveLeft)];
        const bool right = info.m_real_time_actions[static_cast<uint8_t>(Action::kMoveRight)];
        const bool up = info.m_real_time_actions[static_cast<uint8_t>(Action::kMoveUp)];

        // --- Rotation (only allowed while the aircraft is moving) ---
        float speed = std::sqrt(info.m_velocity.x * info.m_velocity.x + info.m_velocity.y * info.m_velocity.y);
        if (speed > 0.f)
        {
            if (left)
            {
                info.m_rotation -= kRotationSpeed;
            }
            if (right)
            {
                info.m_rotation += kRotationSpeed;
            }

            if (left || right)
            {
                // Normalize rotation into [0, 360)
                while (info.m_rotation < 0.f) info.m_rotation += 360.f;
                while (info.m_rotation >= 360.f) info.m_rotation -= 360.f;

                // Align velocity to the new heading, preserving speed magnitude
                if (speed >= kMinSpeedThreshold)
                {
                    double radians = Utility::toRadians(info.m_rotation + 90.f);
                    float dirX = -static_cast<float>(std::cos(radians));
                    float dirY = -static_cast<float>(std::sin(radians));
                    info.m_velocity = sf::Vector2f(dirX * speed, dirY * speed);

                    // Keep the deceleration baseline aligned to the new heading so
                    // that a coasting aircraft continues to travel along its nose
                    // after turning, instead of drifting in the stale release
                    // direction captured when forward was last released.
                    float releaseSpeed = std::sqrt(
                        info.m_velocity_at_release.x * info.m_velocity_at_release.x +
                        info.m_velocity_at_release.y * info.m_velocity_at_release.y);
                    if (releaseSpeed >= kMinSpeedThreshold)
                    {
                        info.m_velocity_at_release = sf::Vector2f(dirX * releaseSpeed, dirY * releaseSpeed);
                    }
                }
            }
        }

        // --- Forward acceleration / deceleration ---
        if (up)
        {
            double radians = Utility::toRadians(info.m_rotation + 90.f);
            float dirX = -static_cast<float>(std::cos(radians));
            float dirY = -static_cast<float>(std::sin(radians));

            float currentSpeed = std::sqrt(info.m_velocity.x * info.m_velocity.x + info.m_velocity.y * info.m_velocity.y);

            info.m_forward_time += dt;
            float holdTime = info.m_forward_time.asSeconds();

            float acceleration = accelerationRate * dt_seconds;
            if (holdTime > boostThreshold)
            {
                acceleration = boostedAccelerationRate * dt_seconds;
            }

            if (currentSpeed < kAircraftMaxSpeed)
            {
                float newSpeed = std::min(currentSpeed + acceleration, kAircraftMaxSpeed);
                info.m_velocity = sf::Vector2f(newSpeed * dirX, newSpeed * dirY);
            }
            else
            {
                info.m_velocity = sf::Vector2f(kAircraftMaxSpeed * dirX, kAircraftMaxSpeed * dirY);
            }

            info.m_was_forward_pressed = true;
        }
        else
        {
            // Detect the transition from pressed to released to capture the deceleration baseline
            if (info.m_was_forward_pressed)
            {
                info.m_forward_time = sf::Time::Zero;
                info.m_release_time = sf::Time::Zero;
                info.m_velocity_at_release = info.m_velocity;
            }

            info.m_release_time += dt;
            float releaseTime = info.m_release_time.asSeconds();

            if (releaseTime >= 0.5f && releaseTime < 3.0f)
            {
                float decelerationProgress = (releaseTime - 0.5f) / 2.5f;
                float decelerationFactor = 1.0f - decelerationProgress;
                info.m_velocity = sf::Vector2f(
                    info.m_velocity_at_release.x * decelerationFactor,
                    info.m_velocity_at_release.y * decelerationFactor);
            }
            else if (releaseTime >= 3.0f)
            {
                info.m_velocity = sf::Vector2f(0.f, 0.f);
            }

            info.m_was_forward_pressed = false;
        }

        // --- Integrate position from velocity ---
        info.m_position += info.m_velocity * dt_seconds;

        // --- Wall bounce: clamp inside battlefield and invert velocity component ---
        bool bounced = false;
        if (info.m_position.x - kAircraftHalfSize <= left_bound)
        {
            info.m_position.x = left_bound + kAircraftHalfSize;
            info.m_velocity.x = -info.m_velocity.x;
            info.m_velocity_at_release.x = -info.m_velocity_at_release.x;
            bounced = true;
        }
        else if (info.m_position.x + kAircraftHalfSize >= right_bound)
        {
            info.m_position.x = right_bound - kAircraftHalfSize;
            info.m_velocity.x = -info.m_velocity.x;
            info.m_velocity_at_release.x = -info.m_velocity_at_release.x;
            bounced = true;
        }

        if (info.m_position.y - kAircraftHalfSize <= top_bound)
        {
            info.m_position.y = top_bound + kAircraftHalfSize;
            info.m_velocity.y = -info.m_velocity.y;
            info.m_velocity_at_release.y = -info.m_velocity_at_release.y;
            bounced = true;
        }
        else if (info.m_position.y + kAircraftHalfSize >= bottom_bound)
        {
            info.m_position.y = bottom_bound - kAircraftHalfSize;
            info.m_velocity.y = -info.m_velocity.y;
            info.m_velocity_at_release.y = -info.m_velocity_at_release.y;
            bounced = true;
        }
        if (bounced)
        {
            float bounceSpeed = std::sqrt(
                info.m_velocity.x * info.m_velocity.x +
                info.m_velocity.y * info.m_velocity.y);
            if (bounceSpeed >= kMinSpeedThreshold)
            {
                // Inverse of dir = -(cos, sin)(rotation + 90 deg)
                double radians = std::atan2(-info.m_velocity.y, -info.m_velocity.x);
                info.m_rotation = static_cast<float>(Utility::ToDegrees(radians)) - 90.f;
                while (info.m_rotation < 0.f) info.m_rotation += 360.f;
                while (info.m_rotation >= 360.f) info.m_rotation -= 360.f;
            }
        }
    }
}

void GameServer::Tick()
{
    // Send state updates at 20 Hz
    UpdateClientState();
    FlushPickupBatch();  // Flush any queued pickup spawns
    FlushPickupDespawns();  // Flush any queued pickup despawns

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
        // The server is authoritative over aircraft positions/rotations: it
        // computes them from player inputs in SimulateMovement. Client-reported
        // positions are NOT trusted, so we parse the packet only to keep the
        // byte stream aligned and then discard the values.
        uint8_t num_aircraft;
        packet >> num_aircraft;

        for (uint8_t i = 0; i < num_aircraft; ++i)
        {
            uint8_t aircraft_identifier;
            sf::Vector2f aircraft_position;
            float rotation;
            packet >> aircraft_identifier >> aircraft_position.x >> aircraft_position.y >> rotation;
            // Intentionally ignored - authoritative state lives on the server.
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
    // The server is authoritative over movement, so it broadcasts the computed
    // state of EVERY aircraft to EVERY peer, including each peer's own aircraft.
    // Clients apply these positions to all aircraft (their own included) rather
    // than deciding their own position locally.
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kUpdateClientState);
    packet << static_cast<uint8_t>(m_aircraft_info.size());

    for (const auto& [id, aircraft] : m_aircraft_info)
    {
        packet
            << id
            << aircraft.m_position.x
            << aircraft.m_position.y
            << aircraft.m_rotation;
    }

    SendToAll(packet);
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
