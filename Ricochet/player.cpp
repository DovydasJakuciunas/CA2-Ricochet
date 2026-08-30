#include "player.hpp"
#include "aircraft.hpp"
#include "movement_controller.hpp"
#include "weapon_system.hpp"
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
{
    // Player 1 Key Bindings (WASD + Space/M)
    m_key_binding_p1[sf::Keyboard::Scancode::A] = Action::kMoveLeft;
    m_key_binding_p1[sf::Keyboard::Scancode::D] = Action::kMoveRight;
    m_key_binding_p1[sf::Keyboard::Scancode::W] = Action::kMoveUp;
    m_key_binding_p1[sf::Keyboard::Scancode::E] = Action::kBulletFire;
    m_key_binding_p1[sf::Keyboard::Scancode::Q] = Action::kMissileFire;

    // Player 2 Key Bindings (Arrow Keys + Numpad 0/Numpad ./Numpad Decimal)
    m_key_binding_p2[sf::Keyboard::Scancode::Left] = Action::kMoveLeft;
    m_key_binding_p2[sf::Keyboard::Scancode::Right] = Action::kMoveRight;
    m_key_binding_p2[sf::Keyboard::Scancode::Up] = Action::kMoveUp;
    m_key_binding_p2[sf::Keyboard::Scancode::Numpad0] = Action::kBulletFire;
    m_key_binding_p2[sf::Keyboard::Scancode::NumpadDecimal] = Action::kMissileFire;

    InitialiseActions();

    // Don't set category here - it will be set based on player_id when commands are created
    m_was_forward_pressed_p1 = false;
    m_was_forward_pressed_p2 = false;

    // Initialize kill counts
    m_player1_kills = 0;
    m_player2_kills = 0;
}

void Player::HandleEvent(const sf::Event& event, CommandQueue& command_queue, PlayerID player_id)
{
    const auto* key_pressed = event.getIf<sf::Event::KeyPressed>();
    if (key_pressed)
    {
        std::map<sf::Keyboard::Scancode, Action>& key_binding = 
            (player_id == PlayerID::kPlayer1) ? m_key_binding_p1 : m_key_binding_p2;

        auto found = key_binding.find(key_pressed->scancode);
        if (found != key_binding.end() && !IsRealTimeAction(found->second))
        {
            Command cmd = m_action_binding[found->second];
            // Set category based on which player this is for
            cmd.category = (player_id == PlayerID::kPlayer1) ?
                static_cast<unsigned int>(ReceiverCategories::kPlayer1Aircraft) :
                static_cast<unsigned int>(ReceiverCategories::kPlayer2Aircraft);
            command_queue.Push(cmd);
        }
    }
}

void Player::HandleRealTimeInput(CommandQueue& command_queue, PlayerID player_id)
{
    std::map<sf::Keyboard::Scancode, Action>& key_binding = 
        (player_id == PlayerID::kPlayer1) ? m_key_binding_p1 : m_key_binding_p2;

    bool& was_forward_pressed = (player_id == PlayerID::kPlayer1) ? m_was_forward_pressed_p1 : m_was_forward_pressed_p2;

    // Find the forward key by looking up which key is bound to kMoveUp
    sf::Keyboard::Scancode forward_key = sf::Keyboard::Scancode::Unknown;
    for (auto pair : key_binding)
    {
        if (pair.second == Action::kMoveUp)
        {
            forward_key = pair.first;
            break;
        }
    }

    bool is_forward_currently_pressed = (forward_key != sf::Keyboard::Scancode::Unknown) && 
                                        sf::Keyboard::isKeyPressed(forward_key);

    // Determine category for this player
    unsigned int player_category = (player_id == PlayerID::kPlayer1) ?
        static_cast<unsigned int>(ReceiverCategories::kPlayer1Aircraft) :
        static_cast<unsigned int>(ReceiverCategories::kPlayer2Aircraft);

    for (auto pair : key_binding)
    {
        if (sf::Keyboard::isKeyPressed(pair.first) && IsRealTimeAction(pair.second))
        {
            Command cmd = m_action_binding[pair.second];
            cmd.category = player_category;
            command_queue.Push(cmd);
        }
    }

    // Reset forward acceleration time if forward key is not pressed but was pressed before
    if (!is_forward_currently_pressed && was_forward_pressed)
    {
        Command reset_command;
        reset_command.category = player_category;
        reset_command.action = DerivedAction<Aircraft>(AircraftForwardAccelerationReset());
        command_queue.Push(reset_command);
    }

    // Apply deceleration when forward key is not pressed
    if (!is_forward_currently_pressed)
    {
        Command decelerate_command;
        decelerate_command.category = player_category;
        decelerate_command.action = DerivedAction<Aircraft>(AircraftDecelerator());
        command_queue.Push(decelerate_command);
    }

    was_forward_pressed = is_forward_currently_pressed;
}

void Player::AssignKey(Action action, sf::Keyboard::Scancode key, PlayerID player_id)
{
    std::map<sf::Keyboard::Scancode, Action>& key_binding = 
        (player_id == PlayerID::kPlayer1) ? m_key_binding_p1 : m_key_binding_p2;

    for (auto itr = key_binding.begin(); itr != key_binding.end();)
    {
        if (itr->second == action)
        {
            key_binding.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
    key_binding[key] = action;
}

sf::Keyboard::Scancode Player::GetAssignedKey(Action action, PlayerID player_id) const
{
    const std::map<sf::Keyboard::Scancode, Action>& key_binding = 
        (player_id == PlayerID::kPlayer1) ? m_key_binding_p1 : m_key_binding_p2;

    for (auto pair : key_binding)
    {
        if (pair.second == action)
        {
            return pair.first;
        }
    }
    return sf::Keyboard::Scancode::Unknown;
}

void Player::SetMissionStatus(MissionStatus status, PlayerID player_id)
{
    if (player_id == PlayerID::kPlayer1)
    {
        m_current_mission_status_p1 = status;
    }
    else
    {
        m_current_mission_status_p2 = status;
    }
}

MissionStatus Player::GetMissionStatus(PlayerID player_id) const
{
    return (player_id == PlayerID::kPlayer1) ? m_current_mission_status_p1 : m_current_mission_status_p2;
}

void Player::SetPlayer1Kills(int kills)
{
    m_player1_kills = kills;
}

void Player::SetPlayer2Kills(int kills)
{
    m_player2_kills = kills;
}

int Player::GetPlayer1Kills() const
{
    return m_player1_kills;
}

int Player::GetPlayer2Kills() const
{
    return m_player2_kills;
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
