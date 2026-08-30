#include "movement_controller.hpp"
#include "aircraft.hpp"
#include "utility.hpp"
#include <cmath>

namespace
{
	const float kMinSpeedThreshold = 0.1f;
	const float kRotationAdjustment = 90.f;
	const float kRotationInvertAdjustment = 180.f;
	const float kFullRotation = 360.f;
}

MovementController::MovementController(Aircraft* aircraft)
	: m_aircraft(aircraft)
	, m_forward_acceleration_time(sf::Time::Zero)
	, m_release_time(sf::Time::Zero)
	, m_velocity_at_release(0.f, 0.f)
{
}

void MovementController::IncrementForwardTime(sf::Time dt)
{
	m_forward_acceleration_time += dt;
}

void MovementController::ResetForwardTime()
{
	m_forward_acceleration_time = sf::Time::Zero;
}

sf::Time MovementController::GetForwardAccelerationTime() const
{
	return m_forward_acceleration_time;
}

void MovementController::IncrementReleaseTime(sf::Time dt)
{
	m_release_time += dt;
}

void MovementController::ResetReleaseTime()
{
	m_release_time = sf::Time::Zero;
}

sf::Time MovementController::GetReleaseTime() const
{
	return m_release_time;
}

void MovementController::StoreVelocityAtRelease()
{
	m_velocity_at_release = m_aircraft->GetVelocity();
}

sf::Vector2f MovementController::GetVelocityAtRelease() const
{
	return m_velocity_at_release;
}

void MovementController::InvertVelocityX()
{
	sf::Vector2f velocity = m_aircraft->GetVelocity();
	m_aircraft->SetVelocity(-velocity.x, velocity.y);
}

void MovementController::InvertVelocityY()
{
	sf::Vector2f velocity = m_aircraft->GetVelocity();
	m_aircraft->SetVelocity(velocity.x, -velocity.y);
}

void MovementController::InvertRotation()
{
	float currentRotation = m_aircraft->getRotation().asDegrees();
	float invertedRotation = currentRotation + kRotationInvertAdjustment;

	if (invertedRotation >= kFullRotation)
	{
		invertedRotation -= kFullRotation;
	}

	m_aircraft->setRotation(sf::degrees(invertedRotation));
}

void MovementController::AlignVelocityToRotation()
{
	sf::Vector2f currentVelocity = m_aircraft->GetVelocity();
	float speed = std::sqrt(currentVelocity.x * currentVelocity.x + currentVelocity.y * currentVelocity.y);

	if (speed < kMinSpeedThreshold)
	{
		return;
	}

	float rotationDegrees = m_aircraft->getRotation().asDegrees();
	double radians = Utility::toRadians(rotationDegrees + kRotationAdjustment);

	float dirX = -std::cos(radians);
	float dirY = -std::sin(radians);

	m_aircraft->SetVelocity(dirX * speed, dirY * speed);
}

void MovementController::AlignRotationToDirection()
{
	sf::Vector2f velocity = m_aircraft->GetVelocity();
	if (velocity.x == 0.f && velocity.y == 0.f)
	{
		return;
	}

	float angle = std::atan2(velocity.y, velocity.x) * 180.f / 3.14159265f;
	angle = angle - 90.f;

	m_aircraft->setRotation(sf::degrees(angle));
}
