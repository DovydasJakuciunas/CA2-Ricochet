#pragma once
#include "entity.hpp"
#include "aircraft_type.hpp"
#include "resource_identifiers.hpp"
#include "text_node.hpp"
#include "projectile_type.hpp"
#include "command_queue.hpp"
#include "animation.hpp"
#include "player.hpp"
#include <memory>
#include <SFML/Window/Keyboard.hpp>

class WeaponSystem;
class MovementController;

class Aircraft : public Entity
{
public:
	Aircraft(AircraftType type, const TextureHolder& textures, const FontHolder& fonts, PlayerID player_id = PlayerID::kPlayer1);
	~Aircraft();
	unsigned int GetCategory() const override;

	void UpdateTexts();

	float GetMaxSpeed() const;
	sf::FloatRect GetBoundingRect() const override;
	bool IsMarkedForRemoval() const override;

	// Player ID accessors
	PlayerID GetPlayerID() const;
	void SetPlayerID(PlayerID player_id);

	// Get aircraft type
	AircraftType GetAircraftType() const;

	// Collision immunity grace period
	void SetCollisionImmunity(sf::Time duration);
	bool IsCollisionImmune() const;

	// Respawn method
	void Respawn();

	// Sprite access for weapon system
	const sf::Sprite& GetSprite() const;

	// Subsystem access
	WeaponSystem& GetWeaponSystem();
	const WeaponSystem& GetWeaponSystem() const;
	MovementController& GetMovementController();
	const MovementController& GetMovementController() const;

private:
	virtual void DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
	virtual void UpdateCurrent(sf::Time dt, CommandQueue& commands) override;

	void UpdateRollAnimation();
	bool IsAllied() const;

private:
	AircraftType m_type;
	sf::Sprite m_sprite;
	Animation m_explosion;

	TextNode* m_health_display;
	TextNode* m_missile_display;

	float m_distance_travelled;
	int m_directions_index;

	bool m_is_marked_for_removal;
	bool m_show_explosion;
	bool m_explosion_began;

	// Player identification
	PlayerID m_player_id;

	// Collision immunity grace period
	sf::Time m_collision_immunity_remaining;

	// Subsystems
	std::unique_ptr<WeaponSystem> m_weapon_system;
	std::unique_ptr<MovementController> m_movement_controller;

};

