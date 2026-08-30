#pragma once
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include "projectile_type.hpp"
#include "command_queue.hpp"
#include "resource_identifiers.hpp"

class SceneNode;
class Aircraft;

class WeaponSystem
{
public:
	WeaponSystem(Aircraft* aircraft, const TextureHolder& textures);

	// Fire control
	void Fire();
	void LaunchMissile();
	void Update(sf::Time dt, CommandQueue& commands);

	// Upgrades
	void IncreaseFireRate();
	void IncreaseFireSpread();
	void CollectMissile(unsigned int count);

	// Getters
	unsigned int GetMissileAmmo() const;
	unsigned int GetFireRate() const;
	unsigned int GetSpreadLevel() const;

	// Projectile creation
	void CreateBullet(SceneNode& node, const TextureHolder& textures);
	void CreateProjectile(SceneNode& node, ProjectileType type, float x_offset, float y_offset, const TextureHolder& textures);
	sf::Vector2f GetBulletSpawnPosition(float x_offset) const;

	// Sound
	void PlayLocalSound(CommandQueue& commands, SoundEffect effect);

private:
	void CheckProjectileLaunch(sf::Time dt, CommandQueue& commands);
	bool IsAllied() const;

	// References
	Aircraft* m_aircraft;
	const TextureHolder& m_textures;

	// Weapon state
	Command m_fire_command;
	Command m_missile_command;

	unsigned int m_fire_rate;
	unsigned int m_spread_level;
	unsigned int m_missile_ammo;

	bool m_is_firing;
	bool m_is_launching_missile;

	sf::Time m_fire_countdown;
};
