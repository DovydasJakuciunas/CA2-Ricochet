#include "player.hpp"
#include "aircraft.hpp"
#include "movement_controller.hpp"
#include "weapon_system.hpp"
#include "gameplay_manager.hpp"
#include <cmath>

struct AircraftMover
{
    AircraftMover(float vx, float vy) : velocity(vx, vy) {}
    void operator()(Aircraft& aircraft, sf::Time) const
    {
        // Prediction only applies to the local player's own aircraft
        if (!aircraft.IsLocallyControlled())
            return;
        aircraft.Accelerate(velocity);
    }

    sf::Vector2f velocity;
};

struct AircraftRotator
{
    AircraftRotator(float rotation) : rotation(rotation) {}
    void operator()(Aircraft& aircraft, sf::Time) const
    {
        // Prediction only applies to the local player's own aircraft
        if (!aircraft.IsLocallyControlled())
            return;

        sf::Vector2f velocity = aircraft.GetVelocity();
        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

        // Only allow rotation when aircraft is moving
        if (speed <= 0.f)
            return;

        aircraft.rotate(sf::degrees(rotation));
        aircraft.GetMovementController().AlignVelocityToRotation();
        aircraft.GetMovementController().StoreVelocityAtRelease();
    }

    float rotation;
};

struct AircraftForwardMover
{
    void operator()(Aircraft& aircraft, sf::Time dt) const
    {
        // Prediction only applies to the local player's own aircraft
        if (!aircraft.IsLocallyControlled())
            return;

        float speed = aircraft.GetMaxSpeed();

        double radians = Utility::toRadians(aircraft.getRotation().asDegrees() + 90.f);
        float dirX = -std::cos(radians);
        float dirY = -std::sin(radians);

        sf::Vector2f currentVelocity = aircraft.GetVelocity();
        float currentSpeed = std::sqrt(currentVelocity.x * currentVelocity.x + currentVelocity.y * currentVelocity.y);

        aircraft.GetMovementController().IncrementForwardTime(dt);

        float holdTime = aircraft.GetMovementController().GetForwardAccelerationTime().asSeconds();

        float deltaTime = dt.asSeconds();
        float acceleration = accelerationRate * deltaTime;

        if (holdTime > boostThreshold)
        {
            acceleration = boostedAccelerationRate * deltaTime;
        }

        if (currentSpeed < speed)
        {
            float newSpeed = std::min(currentSpeed + acceleration, speed);
            aircraft.SetVelocity(newSpeed * dirX, newSpeed * dirY);
        }
        else
        {
            aircraft.SetVelocity(speed * dirX, speed * dirY);
        }
    }
};

struct AircraftForwardAccelerationReset
{
    void operator()(Aircraft& aircraft, sf::Time) const
    {

        if (!aircraft.IsLocallyControlled())
            return;

        aircraft.GetMovementController().ResetForwardTime();
        aircraft.GetMovementController().ResetReleaseTime();
        aircraft.GetMovementController().StoreVelocityAtRelease();
    }
};

struct AircraftDecelerator
{
    void operator()(Aircraft& aircraft, sf::Time dt) const
    {
        // Only mutate the local player's own aircraft
        if (!aircraft.IsLocallyControlled())
            return;

        aircraft.GetMovementController().IncrementReleaseTime(dt);

        float releaseTime = aircraft.GetMovementController().GetReleaseTime().asSeconds();

        if (releaseTime >= 0.5f && releaseTime < 3.0f)
        {
            float decelerationProgress = (releaseTime - 0.5f) / 2.5f;
            float decelerationFactor = 1.0f - decelerationProgress;
            sf::Vector2f initialVelocity = aircraft.GetMovementController().GetVelocityAtRelease();
            aircraft.SetVelocity(initialVelocity.x * decelerationFactor, initialVelocity.y * decelerationFactor);
        }
        else if (releaseTime >= 3.0f)
        {
            aircraft.SetVelocity(0.f, 0.f);
        }
    }
};

Player::Player()
    : m_socket(nullptr)
    , m_identifier(0)
    , m_aircraft_identifier(0)
    , m_key_binding(nullptr)
    , m_gameplay_manager(nullptr)
    , m_current_mission_status(MissionStatus::kMissionRunning)
{
    InitialiseActions();
}

Player::Player(sf::TcpSocket* socket, uint8_t identifier, const KeyBinding* binding, uint8_t aircraft_identifier)
	: m_socket(socket)
	, m_identifier(identifier)
	, m_aircraft_identifier(aircraft_identifier)
	, m_key_binding(binding)
	, m_gameplay_manager(nullptr)
{

	InitialiseActions();

	for (auto& pair : m_action_binding)
	{
		pair.second.category = static_cast<unsigned int>(ReceiverCategories::kPlayerAircraft);
	}
}

void Player::HandleEvent(const sf::Event& event, CommandQueue& command_queue)
{
    const auto* key_pressed = event.getIf<sf::Event::KeyPressed>();
    if (key_pressed && m_key_binding)
    {
        Action action;
        if (m_key_binding->CheckAction(key_pressed->scancode, action) && !IsRealTimeAction(action))
        {
            Command cmd = m_action_binding[action];
            cmd.category = static_cast<unsigned int>(ReceiverCategories::kPlayerAircraft);
            command_queue.Push(cmd);

            // Send player event to server for multiplayer synchronization
            if (m_socket)
            {
                sf::Packet packet;
                packet << static_cast<uint8_t>(Client::PacketType::kPlayerEvent);
                // Send the aircraft_identifier (1-based) so the server can identify the aircraft
                packet << m_aircraft_identifier;
                packet << static_cast<uint8_t>(action);
                m_socket->send(packet);
            }
        }
    }
}

bool Player::IsLocal() const
{
    // IsLocal if we have a key binding (non-network player or local in network game)
    return m_key_binding != nullptr;
}

void Player::SetKeyBinding(const KeyBinding* binding)
{
    m_key_binding = binding;
}


void Player::HandleRealTimeInput(CommandQueue& command_queue)
{
    // Use shared key binding for both single-player and multiplayer
    if (m_key_binding && IsLocal())
    {
        std::vector<Action> activeActions = m_key_binding->GetRealtimeActions();

        // CLIENT-SIDE PREDICTION: apply movement locally for immediate
        // responsiveness. The server is still authoritative and will correct us
        // (see Aircraft reconciliation); we also forward the input below so the
        // server can simulate the same movement.
        for (Action action : activeActions)
        {
            if (action != Action::kBulletFire && action != Action::kMissileFire)
                command_queue.Push(m_action_binding[action]);
        }

        // Track fire button state for network sync (fire on press, not hold)
        bool currentFirePressed = std::find(activeActions.begin(), activeActions.end(), Action::kBulletFire) != activeActions.end();
        bool currentMissilePressed = std::find(activeActions.begin(), activeActions.end(), Action::kMissileFire) != activeActions.end();

        // Fire locally only on button press (transition from not-pressed to pressed)
        if (currentFirePressed && !m_fire_state)
        {
            command_queue.Push(m_action_binding[Action::kBulletFire]);
        }
        m_fire_state = currentFirePressed;

        // Missile locally only on button press (transition from not-pressed to pressed)
        if (currentMissilePressed && !m_missile_state)
        {
            command_queue.Push(m_action_binding[Action::kMissileFire]);
        }
        m_missile_state = currentMissilePressed;

        // Send InputCommand packet only when input state changes
        if (m_socket)
        {
            uint8_t currentFlags = GetCurrentInputFlags();

            // Only send packet if flags changed from previous frame
            if (currentFlags != m_previous_input_flags)
            {
                sf::Packet packet;
                packet << static_cast<uint8_t>(Client::PacketType::kInputCommand);
                packet << m_aircraft_identifier;
                packet << m_input_sequence;
                packet << currentFlags;
                m_socket->send(packet);

                // Increment sequence number only on state change
                ++m_input_sequence;
                m_previous_input_flags = currentFlags;
            }
        }
    }
}


void Player::HandleRealtimeNetworkInput(CommandQueue& commands)
{
    if (m_socket && !IsLocal())
    {
        // Apply fire only when button transitions to pressed (one-shot)
        if (m_action_proxies[Action::kBulletFire])
        {
            commands.Push(m_action_binding[Action::kBulletFire]);
            // Reset so it only fires once per press
            m_action_proxies[Action::kBulletFire] = false;
        }

        // Apply missile only when button transitions to pressed (one-shot)
        if (m_action_proxies[Action::kMissileFire])
        {
            commands.Push(m_action_binding[Action::kMissileFire]);
            // Reset so it only fires once per press
            m_action_proxies[Action::kMissileFire] = false;
        }
    }
}

void Player::AssignKey(Action action, sf::Keyboard::Scancode key)
{
    if (m_key_binding)
    {
        const_cast<KeyBinding*>(m_key_binding)->AssignKey(action, key);
        // Save the updated key bindings to file
        const_cast<KeyBinding*>(m_key_binding)->SaveToFile("keybindings.cfg");
    }
}

sf::Keyboard::Scancode Player::GetAssignedKey(Action action) const
{
    if (m_key_binding)
    {
        return m_key_binding->GetAssignedKey(action);
    }
    return sf::Keyboard::Scancode::Unknown;
}

void Player::SetMissionStatus(MissionStatus status)
{
    m_current_mission_status = status;
}

MissionStatus Player::GetMissionStatus() const
{
    return m_current_mission_status;
}

void Player::SetGameplayManager(GameplayManager* gameplayMgr)
{
    m_gameplay_manager = gameplayMgr;
}

int Player::GetPlayerKills(uint8_t playerID) const
{
    if (m_gameplay_manager)
        return m_gameplay_manager->GetPlayerKills(playerID);
    return 0;
}

const std::map<uint8_t, int>& Player::GetAllPlayerKills() const
{
    if (m_gameplay_manager)
        return m_gameplay_manager->GetAllPlayerKills();

    // Return empty map as fallback (this shouldn't happen in normal gameplay)
    static const std::map<uint8_t, int> empty_map;
    return empty_map;
}

void Player::InitialiseActions()
{
    m_action_binding[Action::kMoveLeft].action = DerivedAction<Aircraft>(AircraftRotator(-kRotationSpeed));
    m_action_binding[Action::kMoveRight].action = DerivedAction<Aircraft>(AircraftRotator(kRotationSpeed));
    m_action_binding[Action::kMoveUp].action = DerivedAction<Aircraft>(AircraftForwardMover());
    uint8_t player_id = m_identifier;
     m_action_binding[Action::kBulletFire].action = DerivedAction<Aircraft>([player_id](Aircraft& a, sf::Time dt)
        {
            // Only execute on the correct aircraft
            if (a.GetPlayerID() == static_cast<PlayerID>(player_id))
            {
                a.GetWeaponSystem().Fire();
            }
        }
    );
    m_action_binding[Action::kMissileFire].action = DerivedAction<Aircraft>([player_id](Aircraft& a, sf::Time dt)
        {
            // Only execute on the correct aircraft
            if (a.GetPlayerID() == static_cast<PlayerID>(player_id))
            {
                a.GetWeaponSystem().LaunchMissile();
            }
        }
    );

}

bool Player::IsRealTimeAction(Action action)
{
    switch (action)
    {
    case Action::kMoveLeft:
    case Action::kMoveRight:
    case Action::kMoveUp:
        return true;

    default:
        return false;
    }
}

void Player::DisableAllRealtimeActions(bool enable)
{
    for (auto& action : m_action_proxies)
    {
        sf::Packet packet{}; // Value-initialized
        packet << static_cast<uint8_t>(Client::PacketType::kPlayerRealtimeChange);
        packet << m_identifier;
        packet << static_cast<uint8_t>(action.first);
        packet << enable;
        m_socket->send(packet);
    }
}

void Player::HandleNetworkEvent(Action action, CommandQueue& commands)
{
    commands.Push(m_action_binding[action]);
}

void Player::HandleNetworkRealtimeChange(Action action, bool actionEnabled)
{
    m_action_proxies[action] = actionEnabled;
}

uint8_t Player::ActionToInputFlag(Action action) const
{
    switch (action)
    {
    case Action::kMoveLeft:
        return (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveLeft));
    case Action::kMoveRight:
        return (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveRight));
    case Action::kMoveUp:
        return (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMoveUp));
    case Action::kBulletFire:
        return (1 << static_cast<uint8_t>(InputCommand::InputFlag::kBulletFire));
    case Action::kMissileFire:
        return (1 << static_cast<uint8_t>(InputCommand::InputFlag::kMissileFire));
    default:
        return 0;
    }
}

uint8_t Player::GetCurrentInputFlags() const
{
    uint8_t flags = 0;
    if (m_key_binding)
    {
        std::vector<Action> activeActions = m_key_binding->GetRealtimeActions();
        for (Action action : activeActions)
        {
            // Only encode movement actions in the flags, not fire/missile
            if (action != Action::kBulletFire && action != Action::kMissileFire)
            {
                flags |= ActionToInputFlag(action);
            }
        }
        // Encode fire and missile states
        if (m_fire_state)
            flags |= ActionToInputFlag(Action::kBulletFire);
        if (m_missile_state)
            flags |= ActionToInputFlag(Action::kMissileFire);
    }
    return flags;
}

