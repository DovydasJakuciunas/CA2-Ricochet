#pragma once
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>

class Aircraft;

class MovementController
{
public:
	MovementController(Aircraft* aircraft);

	// Timing and state management
	void IncrementForwardTime(sf::Time dt);
	void ResetForwardTime();
	sf::Time GetForwardAccelerationTime() const;

	void IncrementReleaseTime(sf::Time dt);
	void ResetReleaseTime();
	sf::Time GetReleaseTime() const;

	// Velocity state
	void StoreVelocityAtRelease();
	sf::Vector2f GetVelocityAtRelease() const;

	// Velocity manipulation
	void InvertVelocityX();
	void InvertVelocityY();

	// Rotation manipulation
	void InvertRotation();
	void AlignVelocityToRotation();
	void AlignRotationToDirection();

private:
	// References
	Aircraft* m_aircraft;

	// Timing state
	sf::Time m_forward_acceleration_time;
	sf::Time m_release_time;

	// Velocity state
	sf::Vector2f m_velocity_at_release;
};
