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
#include <deque>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/System/Clock.hpp>

class WeaponSystem;
class MovementController;
class KeyBinding;

// Network snapshot for interpolation
struct NetworkSnapshot
{
	sf::Vector2f position;
	float rotation;  // in degrees
	sf::Time local_time;  // local time when snapshot was captured

	NetworkSnapshot(sf::Vector2f pos, float rot, sf::Time time)
		: position(pos), rotation(rot), local_time(time) {}
};

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

	// Set key binding for animation updates
	void SetKeyBinding(const KeyBinding* binding);

	// Deceleration when forward key is released
	void ApplyDeceleration(sf::Time dt);

	// Network snapshot buffering
	void AddNetworkSnapshot(sf::Vector2f position, float rotation);
	void UpdateNetworkInterpolation(sf::Time dt);

	// Client-side prediction + reconciliation (local player's aircraft)
	void SetLocallyControlled(bool locally_controlled);
	bool IsLocallyControlled() const;
	void ApplyServerCorrection(sf::Vector2f position, float rotation);

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

	// Track forward key state for deceleration transition
	bool m_was_forward_pressed;

	sf::Time m_collision_immunity_remaining;

	// Subsystems
	std::unique_ptr<WeaponSystem> m_weapon_system;
	std::unique_ptr<MovementController> m_movement_controller;

	// Key binding for animation
	const KeyBinding* m_key_binding;

	// Network snapshot buffering (~100ms history)
	std::deque<NetworkSnapshot> m_snapshot_buffer;
	sf::Clock m_snapshot_clock;  // local time for lookback
	static constexpr float SNAPSHOT_BUFFER_MAX_SIZE = 10;  // ~1 second @ 10Hz
	static constexpr int INTERPOLATION_DELAY_MS = 100;  // Display time 100ms in the past

	// Client-side prediction + reconciliation state (local player's aircraft only)
	bool m_is_locally_controlled = false;   // true for the local player's own aircraft
	bool m_has_server_correction = false;   // true once an authoritative update has arrived
	sf::Vector2f m_server_position;          // latest authoritative position
	float m_server_rotation = 0.f;           // latest authoritative rotation (degrees)

};

