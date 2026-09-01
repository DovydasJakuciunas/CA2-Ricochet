#include "aircraft.hpp"
#include "weapon_system.hpp"
#include "movement_controller.hpp"
#include "key_binding.hpp"
#include "texture_id.hpp"
#include "data_tables.hpp"
#include "constants.hpp"
#include "utility.hpp"
#include <iostream>

namespace
{
	const std::vector<AircraftData> Table = InitializeAircraftData();

	// Explosion animation constants
	const int kExplosionFrameSize = 256;
	const int kExplosionFrameCount = 16;
	const float kExplosionDuration = 1.0f;
}

TextureID ToTextureID(AircraftType type)
{
	switch (type)
	{
	case AircraftType::kEagle:
		return TextureID::kEagle;
		break;
	case AircraftType::kRaptor:
		return TextureID::kRaptor;
		break;
	}
	return TextureID::kEagle;
}

Aircraft::Aircraft(AircraftType type, const TextureHolder& textures, const FontHolder& fonts, PlayerID player_id)
	: Entity(Table[static_cast<int>(type)].m_hitpoints)
	, m_type(type)
	, m_sprite(textures.Get(Table[static_cast<int>(type)].m_texture), Table[static_cast<int>(type)].m_texture_rect)
	, m_health_display(nullptr)
	, m_missile_display(nullptr)
	, m_distance_travelled(0.f)
	, m_directions_index(0)
	, m_is_marked_for_removal(false)
	, m_show_explosion(true)
	, m_explosion(textures.Get(TextureID::kExplosion))
	, m_explosion_began(false)
	, m_player_id(player_id)
	, m_collision_immunity_remaining(sf::Time::Zero)
	, m_was_forward_pressed(false)
	, m_weapon_system(std::make_unique<WeaponSystem>(this, textures))
	, m_movement_controller(std::make_unique<MovementController>(this))
	, m_key_binding(nullptr)
{
	m_explosion.SetFrameSize(sf::Vector2i(kExplosionFrameSize, kExplosionFrameSize));
	m_explosion.SetNumFrames(kExplosionFrameCount);
	m_explosion.SetDuration(sf::seconds(kExplosionDuration));
	Utility::CentreOrigin(m_sprite);
	Utility::CentreOrigin(m_explosion);

	std::string* health = new std::string("");
	std::unique_ptr<TextNode> health_display(new TextNode(fonts, *health));
	m_health_display = health_display.get();
	AttachChild(std::move(health_display));

	// Player aircraft should have missile display
	if (m_player_id == PlayerID::kPlayer1)
	{
		std::string* missile_ammo = new std::string("");
		std::unique_ptr<TextNode> missile_display(new TextNode(fonts, *missile_ammo));
		m_missile_display = missile_display.get();
		AttachChild(std::move(missile_display));
	}
	UpdateTexts();
}

Aircraft::~Aircraft()
{
}

unsigned int Aircraft::GetCategory() const
{
	// Return universal player aircraft category
	return static_cast<unsigned int>(ReceiverCategories::kPlayerAircraft);
}

void Aircraft::UpdateTexts()
{
	m_health_display->SetString(std::to_string(GetHitPoints()) + "HP");
	m_health_display->setPosition(sf::Vector2f(0.f, 50.f));
	m_health_display->setRotation(-getRotation());

	if (m_missile_display)
	{
		m_missile_display->setPosition(sf::Vector2f(0.f, 65.f));
		m_missile_display->setRotation(-getRotation());
		if (m_weapon_system->GetMissileAmmo() == 0)
		{
			m_missile_display->SetString("");
		}
		else
		{
			m_missile_display->SetString("M: " + std::to_string(m_weapon_system->GetMissileAmmo()));
		}
	}
}

float Aircraft::GetMaxSpeed() const
{
	return Table[static_cast<int>(m_type)].m_speed;
}

sf::FloatRect Aircraft::GetBoundingRect() const
{
	return GetWorldTransform().transformRect(m_sprite.getGlobalBounds());
}

bool Aircraft::IsMarkedForRemoval() const
{
	return IsDestroyed() && (m_explosion.IsFinished() || !m_show_explosion);
}

void Aircraft::DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
	if (IsDestroyed() && m_show_explosion)
	{
		target.draw(m_explosion, states);
	}
	else
	{
		// Apply color tint based on player ID
		sf::Sprite coloredSprite = m_sprite;
		coloredSprite.setColor(PlayerColors::GetColor(static_cast<unsigned int>(m_player_id)));
		target.draw(coloredSprite, states);
	}
}

void Aircraft::UpdateCurrent(sf::Time dt, CommandQueue& commands)
{
	if (IsDestroyed())
	{
		m_explosion.Update(dt);
		//Play explosion sound only once
		if (!m_explosion_began)
		{
			SoundEffect soundEffect = (Utility::RandomInt(2) == 0) ? SoundEffect::kExplosion1 : SoundEffect::kExplosion2;
			m_weapon_system->PlayLocalSound(commands, soundEffect);
			m_explosion_began = true;
		}
		m_is_marked_for_removal = true;
		return;
	}

	// Decrement collision immunity timer
	if (m_collision_immunity_remaining > sf::Time::Zero)
	{
		m_collision_immunity_remaining -= dt;
	}

	if (m_is_locally_controlled)
	{
		// --- CLIENT-SIDE PREDICTION ---
		// The local player's input has already set this aircraft's velocity via
		// the movement commands pushed by Player::HandleRealTimeInput. We simulate
		// it locally for immediate responsiveness; Entity::UpdateCurrent below
		// integrates the predicted velocity. Deceleration is part of the model.
		if (m_key_binding)
		{
			bool isForwardPressed = sf::Keyboard::isKeyPressed(m_key_binding->GetAssignedKey(Action::kMoveUp));

			// Detect transition from pressed to released
			if (m_was_forward_pressed && !isForwardPressed)
			{
				m_movement_controller->ResetForwardTime();
				m_movement_controller->ResetReleaseTime();
				m_movement_controller->StoreVelocityAtRelease();
			}

			if (!isForwardPressed)
			{
				ApplyDeceleration(dt);
			}

			// Update state for next frame
			m_was_forward_pressed = isForwardPressed;
		}
	}
	else
	{
		// Remote aircraft follow authoritative server snapshots via interpolation
		UpdateNetworkInterpolation(dt);
	}

	Entity::UpdateCurrent(dt, commands);

	// --- RECONCILIATION ---
	// After predicting locally, gently correct the local aircraft toward the
	// authoritative server state. Small errors are smoothed out over time to
	// avoid rubber-banding; large divergences are snapped immediately.
	if (m_is_locally_controlled && m_has_server_correction)
	{
		sf::Vector2f current = getPosition();
		sf::Vector2f error = m_server_position - current;
		float distance = std::sqrt(error.x * error.x + error.y * error.y);

		const float kSnapThreshold = 150.f;   // large divergence -> snap
		const float kDeadZone = 40.f;         // ignore lag-expected error
		const float kCorrectionRate = 4.f;    // blend speed (fraction/sec)

		if (distance > kSnapThreshold)
		{
			setPosition(m_server_position);
			setRotation(sf::degrees(m_server_rotation));
		}
		else if (distance > kDeadZone)
		{
			//Allows the server to attempt to correct the position of the aircraft without causing jittery movement
			float excess = distance - kDeadZone;
			sf::Vector2f direction = error / distance;
			float t = std::min(1.f, kCorrectionRate * dt.asSeconds());
			setPosition(current + direction * (excess * t));
		}
		else
		{
			float t = std::min(1.f, kCorrectionRate * dt.asSeconds());
			float cur_rot = getRotation().asDegrees();
			float delta = m_server_rotation - cur_rot;
			while (delta > 180.f) delta -= 360.f;
			while (delta < -180.f) delta += 360.f;
			setRotation(sf::degrees(cur_rot + delta * t));
		}
	}

	UpdateTexts();

	UpdateRollAnimation();

	// Update weapon system
	m_weapon_system->Update(dt, commands);
}

bool Aircraft::IsAllied() const
{
	return m_type == AircraftType::kEagle;
}

void Aircraft::UpdateRollAnimation()
{
	if (Table[static_cast<int>(m_type)].m_has_roll_animation)
	{
		sf::IntRect textureRect = Table[static_cast<int>(m_type)].m_texture_rect;

		bool is_left_pressed = false;
		bool is_right_pressed = false;

		// If we have a key binding, use the realtime actions
		if (m_key_binding)
		{
			std::vector<Action> activeActions = m_key_binding->GetRealtimeActions();

			for (Action action : activeActions)
			{
				if (action == Action::kMoveLeft)
					is_left_pressed = true;
				else if (action == Action::kMoveRight)
					is_right_pressed = true;
			}
		}
		else
		{
			// Fallback to hardcoded A/D keys if binding not available
			is_left_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A);
			is_right_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D);
		}

		// Roll animation based on bound key input
		if (is_left_pressed)
		{
			textureRect.position.x += textureRect.size.x;  // Roll left
		}
		else if (is_right_pressed)
		{
			textureRect.position.x += 2 * textureRect.size.x;  // Roll right
		}
		// No tilt: straight when neither key is pressed

		m_sprite.setTextureRect(textureRect);
	}
}

PlayerID Aircraft::GetPlayerID() const
{
	return m_player_id;
}

void Aircraft::SetPlayerID(PlayerID player_id)
{
	m_player_id = player_id;
}

const sf::Sprite& Aircraft::GetSprite() const
{
	return m_sprite;
}

AircraftType Aircraft::GetAircraftType() const
{
	return m_type;
}

void Aircraft::SetCollisionImmunity(sf::Time duration)
{
	m_collision_immunity_remaining = duration;
}

bool Aircraft::IsCollisionImmune() const
{
	return m_collision_immunity_remaining > sf::Time::Zero;
}

void Aircraft::Respawn()
{
	// Reset health to full
	Repair(100);
	// Reset velocity
	SetVelocity(0.f, 0.f);
	// Clear destruction state
	m_is_marked_for_removal = false;
	m_show_explosion = true;
	m_explosion_began = false;
	// Reset explosion animation
	m_explosion.Restart();
}

WeaponSystem& Aircraft::GetWeaponSystem()
{
	return *m_weapon_system;
}

const WeaponSystem& Aircraft::GetWeaponSystem() const
{
	return *m_weapon_system;
}

MovementController& Aircraft::GetMovementController()
{
	return *m_movement_controller;
}

const MovementController& Aircraft::GetMovementController() const
{
	return *m_movement_controller;
}

void Aircraft::SetKeyBinding(const KeyBinding* binding)
{
	m_key_binding = binding;
}

void Aircraft::SetLocallyControlled(bool locally_controlled)
{
	m_is_locally_controlled = locally_controlled;
}

bool Aircraft::IsLocallyControlled() const
{
	return m_is_locally_controlled;
}

void Aircraft::ApplyServerCorrection(sf::Vector2f position, float rotation)
{
	if (m_has_server_correction)
	{
		float cur_rot = getRotation().asDegrees();
		float delta = rotation - cur_rot;
		while (delta > 180.f) delta -= 360.f;
		while (delta < -180.f) delta += 360.f;

		const float kBounceHeadingThreshold = 60.f; // sharp reversal => bounce
		if (std::abs(delta) > kBounceHeadingThreshold)
		{
			sf::Vector2f velocity = GetVelocity();
			float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
			if (speed > 0.1f)
			{
				// Realign predicted velocity to the authoritative heading,
				// preserving speed (dir = -(cos, sin)(rotation + 90 deg)).
				double radians = Utility::toRadians(static_cast<double>(rotation) + 90.0);
				float dirX = -static_cast<float>(std::cos(radians));
				float dirY = -static_cast<float>(std::sin(radians));
				SetVelocity(dirX * speed, dirY * speed);
				m_movement_controller->StoreVelocityAtRelease();
			}
			setRotation(sf::degrees(rotation));
		}
	}

	m_server_position = position;
	m_server_rotation = rotation;
	m_has_server_correction = true;
}

void Aircraft::ApplyDeceleration(sf::Time dt)
{
	m_movement_controller->IncrementReleaseTime(dt);

	float releaseTime = m_movement_controller->GetReleaseTime().asSeconds();

	if (releaseTime >= 0.5f && releaseTime < 3.0f)
	{
		float decelerationProgress = (releaseTime - 0.5f) / 2.5f;
		float decelerationFactor = 1.0f - decelerationProgress;
		sf::Vector2f initialVelocity = m_movement_controller->GetVelocityAtRelease();
		SetVelocity(initialVelocity.x * decelerationFactor, initialVelocity.y * decelerationFactor);
	}
	else if (releaseTime >= 3.0f)
	{
		SetVelocity(0.f, 0.f);
	}
}

namespace
{
	// Interpolate between two angles using shortest path
	float InterpolateAngle(float start, float end, float t)
	{
		// Normalize angles to [0, 360)
		while (start < 0) start += 360.f;
		while (start >= 360) start -= 360.f;
		while (end < 0) end += 360.f;
		while (end >= 360) end -= 360.f;

		// Calculate shortest angular distance
		float delta = end - start;
		if (delta > 180.f)
			delta -= 360.f;
		else if (delta < -180.f)
			delta += 360.f;

		return start + delta * t;
	}
}

void Aircraft::AddNetworkSnapshot(sf::Vector2f position, float rotation)
{
	// Add snapshot with current local time
	m_snapshot_buffer.emplace_back(position, rotation, m_snapshot_clock.getElapsedTime());

	// Keep buffer size bounded
	while (m_snapshot_buffer.size() > static_cast<size_t>(SNAPSHOT_BUFFER_MAX_SIZE))
	{
		m_snapshot_buffer.pop_front();
	}
}

void Aircraft::UpdateNetworkInterpolation(sf::Time dt)
{
	if (m_snapshot_buffer.size() < 2)
	{
		// Not enough snapshots for interpolation
		return;
	}

	// Calculate target lookup time (now - 100ms)
	sf::Time local_now = m_snapshot_clock.getElapsedTime();
	sf::Time target_time = local_now - sf::milliseconds(INTERPOLATION_DELAY_MS);

	// Find the two snapshots bracketing target_time
	const NetworkSnapshot* before = nullptr;
	const NetworkSnapshot* after = nullptr;

	for (size_t i = 0; i < m_snapshot_buffer.size(); ++i)
	{
		if (m_snapshot_buffer[i].local_time <= target_time)
		{
			before = &m_snapshot_buffer[i];
		}
		if (m_snapshot_buffer[i].local_time >= target_time && after == nullptr)
		{
			after = &m_snapshot_buffer[i];
		}
	}

	// If we found both snapshots, interpolate between them
	if (before && after)
	{
		// Calculate interpolation factor
		sf::Time time_diff = after->local_time - before->local_time;
		float t = 0.f;
		if (time_diff.asMilliseconds() > 0)
		{
			t = (target_time.asSeconds() - before->local_time.asSeconds()) / time_diff.asSeconds();
			t = std::clamp(t, 0.f, 1.f);
		}

		// Linearly interpolate position
		sf::Vector2f interpolated_pos = before->position + (after->position - before->position) * t;
		setPosition(interpolated_pos);

		// Interpolate rotation using shortest angle path
		float interpolated_rot = InterpolateAngle(before->rotation, after->rotation, t);
		setRotation(sf::degrees(interpolated_rot));
	}
	else if (before)
	{
		// Only have "before" snapshot, snap to it
		setPosition(before->position);
		setRotation(sf::degrees(before->rotation));
	}
	else if (after)
	{
		// Only have "after" snapshot, snap to it
		setPosition(after->position);
		setRotation(sf::degrees(after->rotation));
	}
}


