#include "weapon_system.hpp"
#include "aircraft.hpp"
#include "texture_id.hpp"
#include "data_tables.hpp"
#include "constants.hpp"
#include "projectile.hpp"
#include "sound_node.hpp"
#include "receiver_categories.hpp"
#include "utility.hpp"
#include "scene_node.hpp"
#include <cmath>

namespace
{
	const std::vector<AircraftData> Table = InitializeAircraftData();

	// Spread patterns
	const float kSpreadLeftOffset = -0.5f;
	const float kSpreadRightOffset = 0.5f;
	const float kSpreadCenterOffset = 0.0f;

	// Projectile spawn offsets
	const float kBulletYOffset = 0.5f;
	const float kMissileYOffset = 0.5f;

	// Rotation calculations
	const float kRotationAdjustment = 90.f;
}

WeaponSystem::WeaponSystem(Aircraft* aircraft, const TextureHolder& textures)
	: m_aircraft(aircraft)
	, m_textures(textures)
	, m_fire_rate(1)
	, m_spread_level(1)
	, m_missile_ammo(2)
	, m_is_firing(false)
	, m_is_launching_missile(false)
	, m_fire_countdown(sf::Time::Zero)
{
	m_fire_command.category = static_cast<int>(ReceiverCategories::kScene);
	m_fire_command.action = [this](SceneNode& node, sf::Time dt)
	{
		CreateBullet(node, m_textures);
	};

	m_missile_command.category = static_cast<int>(ReceiverCategories::kScene);
	m_missile_command.action = [this](SceneNode& node, sf::Time dt)
	{
		CreateProjectile(node, ProjectileType::kMissile, kSpreadCenterOffset, kMissileYOffset, m_textures);
	};
}

void WeaponSystem::Fire()
{
	if (Table[static_cast<int>(m_aircraft->GetAircraftType())].m_fire_interval != sf::Time::Zero)
	{
		m_is_firing = true;
	}
}

void WeaponSystem::LaunchMissile()
{
	if (m_missile_ammo > 0)
	{
		m_is_launching_missile = true;
		--m_missile_ammo;
	}
}

void WeaponSystem::IncreaseFireRate()
{
	if (m_fire_rate < kMaxFireRate)
	{
		++m_fire_rate;
	}
}

void WeaponSystem::IncreaseFireSpread()
{
	if (m_spread_level < kMaxSpread)
	{
		++m_spread_level;
	}
}

void WeaponSystem::CollectMissile(unsigned int count)
{
	m_missile_ammo += count;
}

unsigned int WeaponSystem::GetMissileAmmo() const
{
	return m_missile_ammo;
}

unsigned int WeaponSystem::GetFireRate() const
{
	return m_fire_rate;
}

unsigned int WeaponSystem::GetSpreadLevel() const
{
	return m_spread_level;
}

void WeaponSystem::Update(sf::Time dt, CommandQueue& commands)
{
	CheckProjectileLaunch(dt, commands);
}

void WeaponSystem::CheckProjectileLaunch(sf::Time dt, CommandQueue& commands)
{
	if (!IsAllied())
	{
		Fire();
	}

	if (m_is_firing && m_fire_countdown <= sf::Time::Zero)
	{
		PlayLocalSound(commands, IsAllied() ? SoundEffect::kEnemyGunfire : SoundEffect::kAlliedGunfire);
		commands.Push(m_fire_command);
		m_fire_countdown += Table[static_cast<int>(m_aircraft->GetAircraftType())].m_fire_interval / (m_fire_rate + 1.f);
	}
	else if (m_fire_countdown > sf::Time::Zero)
	{
		m_fire_countdown -= dt;
		m_is_firing = false;
	}

	// Missile launch
	if (m_is_launching_missile)
	{
		PlayLocalSound(commands, SoundEffect::kLaunchMissile);
		commands.Push(m_missile_command);
	}
}

bool WeaponSystem::IsAllied() const
{
	// For PvP, both players are allies with their own projectiles
	// In the future, this could be based on team/player ID if needed
	// For now, treat all player aircraft as allied
	return m_aircraft->GetPlayerID() == PlayerID::kPlayer1 || m_aircraft->GetPlayerID() == PlayerID::kPlayer2;
}

void WeaponSystem::CreateBullet(SceneNode& node, const TextureHolder& textures)
{
	ProjectileType type = IsAllied() ? ProjectileType::kAlliedBullet : ProjectileType::kEnemyBullet;
	switch (m_spread_level)
	{
	case 1:
		if (m_is_firing)
		{
			CreateProjectile(node, type, kSpreadCenterOffset, kBulletYOffset, textures);
			m_is_firing = false;
		}
		break;
	case 2:
		if (m_is_firing)
		{
			CreateProjectile(node, type, kSpreadLeftOffset, kBulletYOffset, textures);
			CreateProjectile(node, type, kSpreadRightOffset, kBulletYOffset, textures);
			m_is_firing = false;
		}
		break;
	case 3:
		if (m_is_firing)
		{
			CreateProjectile(node, type, kSpreadCenterOffset, kBulletYOffset, textures);
			CreateProjectile(node, type, kSpreadLeftOffset, kBulletYOffset, textures);
			CreateProjectile(node, type, kSpreadRightOffset, kBulletYOffset, textures);
			m_is_firing = false;
		}
		break;
	}
}

void WeaponSystem::CreateProjectile(SceneNode& node, ProjectileType type, float x_offset, float y_offset, const TextureHolder& textures)
{
	if (m_is_launching_missile || m_is_firing)
	{
		std::unique_ptr<Projectile> projectile(new Projectile(type, textures));
		sf::Vector2f spawnPosition = GetBulletSpawnPosition(x_offset);

		// Calculate velocity in the direction the jet is facing
		float rotationDegrees = m_aircraft->getRotation().asDegrees();
		double rotationRadians = Utility::toRadians(rotationDegrees + -90.f);
		float maxSpeed = projectile->GetMaxSpeed();

		float dirX = -std::cos(rotationRadians);
		float dirY = -std::sin(rotationRadians);

		float sign = IsAllied() ? -1.f : 1.f;
		sf::Vector2f velocity(dirX * maxSpeed * sign, dirY * maxSpeed * sign);

		projectile->setPosition(spawnPosition);
		projectile->SetVelocity(velocity);
		projectile->SetOwnerPlayerID(m_aircraft->GetPlayerID());
		node.AttachChild(std::move(projectile));
		m_is_launching_missile = false;
	}
}

sf::Vector2f WeaponSystem::GetBulletSpawnPosition(float x_offset) const
{
	// Get the jet's bounding box and center
	const sf::Sprite& sprite = m_aircraft->GetSprite();
	sf::FloatRect bounds = sprite.getGlobalBounds();
	sf::Vector2f jetCenter = m_aircraft->GetWorldPosition();

	// Calculate the front (nose) of the jet based on rotation
	// The jet's front is perpendicular to its body, pointing in the direction it's facing
	float rotationDegrees = m_aircraft->getRotation().asDegrees();
	double rotationRadians = Utility::toRadians(rotationDegrees + kRotationAdjustment);

	// Distance from center to front of jet (half height)
	float frontDistance = bounds.size.y * 0.5f;

	// Calculate front position based on rotation
	float frontX = -std::cos(rotationRadians) * frontDistance;
	float frontY = -std::sin(rotationRadians) * frontDistance;

	// Apply horizontal spread offset at the front
	float spreadOffsetX = x_offset * bounds.size.x;
	double spreadRotationRadians = Utility::toRadians(rotationDegrees);
	float perpX = -std::cos(spreadRotationRadians) * spreadOffsetX;
	float perpY = -std::sin(spreadRotationRadians) * spreadOffsetX;

	sf::Vector2f spawnPos = jetCenter + sf::Vector2f(frontX + perpX, frontY + perpY);
	return spawnPos;
}

void WeaponSystem::PlayLocalSound(CommandQueue& commands, SoundEffect effect)
{
	sf::Vector2f world_position = m_aircraft->GetWorldPosition();

	Command command;
	command.category = static_cast<int>(ReceiverCategories::kSoundEffect);
	command.action = DerivedAction<SoundNode>(
		[effect, world_position](SoundNode& node, sf::Time)
		{
			node.PlaySound(effect, world_position);
		});
	commands.Push(command);
}
