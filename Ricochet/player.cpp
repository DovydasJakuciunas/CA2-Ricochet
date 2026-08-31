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
        aircraft.Accelerate(velocity);
    }

    sf::Vector2f velocity;
};

struct AircraftRotator
{
    AircraftRotator(float rotation) : rotation(rotation) {}
    void operator()(Aircraft& aircraft, sf::Time) const
    {
        sf::Vector2f velocity = aircraft.GetVelocity();
        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

        if (speed > 0.f)
        {
            aircraft.rotate(sf::degrees(rotation));
            aircraft.GetMovementController().AlignVelocityToRotation();
            aircraft.GetMovementController().StoreVelocityAtRelease();
        }
    }

    float rotation;
};

struct AircraftForwardMover
{
    void operator()(Aircraft& aircraft, sf::Time dt) const
    {
        float speed = aircraft.GetMaxSpeed();

        double radians = Utility::toRadians(aircraft.getRotation().asDegrees() + 90.f);
        float dirX = -std::cos(radians);
        float dirY = -std::sin(radians);

        sf::Vector2f currentVelocity = aircraft.GetVelocity();
        float currentSpeed = std::sqrt(currentVelocity.x * currentVelocity.x + currentVelocity.y * currentVelocity.y);

        aircraft.GetMovementController().IncrementForwardTime(dt);

        float holdTime = aircraft.GetMovementController().GetForwardAccelerationTime().asSeconds();

        const float accelerationRate = 300.f;
        const float boostedAccelerationRate = 15000.f;
        const float boostThreshold = 2.f;

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
        aircraft.GetMovementController().ResetForwardTime();
        aircraft.GetMovementController().ResetReleaseTime();
        aircraft.GetMovementController().StoreVelocityAtRelease();
    }
};

struct AircraftDecelerator
{
    void operator()(Aircraft& aircraft, sf::Time dt) const
    {
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
    , m_key_binding(nullptr)
    , m_gameplay_manager(nullptr)
    , m_current_mission_status(MissionStatus::kMissionRunning)
    , m_was_forward_pressed(false)
{
    InitialiseActions();
}

Player::Player(sf::TcpSocket* socket, uint8_t identifier, const KeyBinding* binding)
	: m_socket(socket)
	, m_identifier(identifier)
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

const std::map<Action, bool>& Player::GetActionProxies() const
{
    return m_action_proxies;
}

std::map<Action, bool> Player::GetChangedActions()
{
    std::map<Action, bool> changed_actions;

    if (!m_key_binding || !IsLocal())
    {
        return changed_actions;
    }

    // Get current keyboard input
    std::vector<Action> current_actions = m_key_binding->GetRealtimeActions();

    // Check for actions that were pressed (in current but not in previous)
    for (const auto& action : current_actions)
    {
        auto prev_it = std::find(m_previous_realtime_actions.begin(), 
                                 m_previous_realtime_actions.end(), 
                                 action);
        if (prev_it == m_previous_realtime_actions.end())
        {
            // Action is new/pressed - send it as true
            changed_actions[action] = true;
            std::cout << "[PLAYER] GetChangedActions - Action PRESSED: " << static_cast<int>(action) << std::endl;
        }
    }

    // Check for actions that were released (in previous but not in current)
    for (const auto& action : m_previous_realtime_actions)
    {
        auto curr_it = std::find(current_actions.begin(), 
                                 current_actions.end(), 
                                 action);
        if (curr_it == current_actions.end())
        {
            // Action is released - send it as false
            changed_actions[action] = false;
            std::cout << "[PLAYER] GetChangedActions - Action RELEASED: " << static_cast<int>(action) << std::endl;
        }
    }

    // Store current state for next frame
    m_previous_realtime_actions = current_actions;

    // Debug: Show how many packets will be sent
    if (!changed_actions.empty())
    {
        std::cout << "[PLAYER] GetChangedActions - Sending " << changed_actions.size() << " packet(s)" << std::endl;
    }

    return changed_actions;
}

void Player::HandleRealTimeInput(CommandQueue& command_queue)
{
    // Use shared key binding for both single-player and multiplayer
    if (m_key_binding && IsLocal())
    {
        // Get active keys from keyboard input
        std::vector<Action> activeActions = m_key_binding->GetRealtimeActions();

        // Debug: Show current active actions
        if (!activeActions.empty())
        {
            std::cout << "[PLAYER] HandleRealTimeInput - Active actions: ";
            for (const auto& action : activeActions)
            {
                std::cout << static_cast<int>(action) << " ";
            }
            std::cout << std::endl;
        }

        // Update action proxies (for display/reference)
        for (const auto& action : activeActions)
        {
            m_action_proxies[action] = true;
        }

        // Push commands to queue (for single-player or local visual feedback)
        for (const auto& action : activeActions)
        {
            command_queue.Push(m_action_binding[action]);
        }
    }
}

void Player::HandleRealtimeNetworkInput(CommandQueue& commands)
{
    if (m_socket && !IsLocal())
    {
        // Traverse all realtime input proxies. Because this is a networked game, the input isn't handled directly
        for (auto pair : m_action_proxies)
        {
            if (pair.second && IsRealtimeAction(pair.first))
                commands.Push(m_action_binding[pair.first]);
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
    m_action_binding[Action::kBulletFire].action = DerivedAction<Aircraft>([](Aircraft& a, sf::Time dt)
        {
            a.GetWeaponSystem().Fire();
        }
    );
    m_action_binding[Action::kMissileFire].action = DerivedAction<Aircraft>([](Aircraft& a, sf::Time dt)
        {
            a.GetWeaponSystem().LaunchMissile();
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
