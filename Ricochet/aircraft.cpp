#include "aircraft.hpp"
#include "weapon_system.hpp"
#include "movement_controller.hpp"
#include "texture_id.hpp"
#include "data_tables.hpp"
#include "constants.hpp"
#include "utility.hpp"

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
	, m_weapon_system(std::make_unique<WeaponSystem>(this, textures))
	, m_movement_controller(std::make_unique<MovementController>(this))
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

	// Both player 1 and player 2 should have missile display
	if (m_player_id == PlayerID::kPlayer1 || m_player_id == PlayerID::kPlayer2)
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
	// Return category based on player ID
	if (m_player_id == PlayerID::kPlayer1)
	{
		return static_cast<unsigned int>(ReceiverCategories::kPlayer1Aircraft);
	}
	return static_cast<unsigned int>(ReceiverCategories::kPlayer2Aircraft);
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
		if (m_player_id == PlayerID::kPlayer1)
		{
			// Player 1: Blue tint
			sf::Sprite blueSpriteVersion = m_sprite;
			blueSpriteVersion.setColor(kPlayer1Color);
			target.draw(blueSpriteVersion, states);
		}
		else if (m_player_id == PlayerID::kPlayer2)
		{
			// Player 2: Red tint
			sf::Sprite redSpriteVersion = m_sprite;
			redSpriteVersion.setColor(kPlayer2Color);
			target.draw(redSpriteVersion, states);
		}
		else
		{
			// Default: white (no tint)
			target.draw(m_sprite, states);
		}
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

	Entity::UpdateCurrent(dt, commands);
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

		// Check which directional keys are being pressed based on player ID
		bool is_left_pressed, is_right_pressed;

		if (m_player_id == PlayerID::kPlayer1)
		{
			// Player 1: A and D keys
			is_left_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A);
			is_right_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D);
		}
		else if (m_player_id == PlayerID::kPlayer2)
		{
			// Player 2: Left and Right arrow keys
			is_left_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Left);
			is_right_pressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Right);
		}
		else
		{
			is_left_pressed = false;
			is_right_pressed = false;
		}

		// Roll animation based on key input
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
