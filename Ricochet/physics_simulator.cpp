#include "physics_simulator.hpp"
#include "aircraft.hpp"
#include "scene_node.hpp"
#include "entity.hpp"
#include "action.hpp"
#include "receiver_categories.hpp"
#include "projectile.hpp"
#include <cmath>

PhysicsSimulator::PhysicsSimulator(const sf::FloatRect& world_bounds, const sf::View& camera)
	: m_world_bounds(world_bounds)
	, m_camera(camera)
{
}

sf::FloatRect PhysicsSimulator::GetViewBounds() const
{
	return sf::FloatRect(m_camera.getCenter() - m_camera.getSize() / 2.f, m_camera.getSize());
}

sf::FloatRect PhysicsSimulator::GetBattleFieldBounds() const
{
	// Return camera bounds + a small area off screen where the enemies spawn
	sf::FloatRect bounds = GetViewBounds();
	bounds.position.y -= 100.f;
	bounds.size.y += 100.f;
	return bounds;
}

void PhysicsSimulator::HandlePlayerBoundaryCollision(Aircraft* player1, Aircraft* player2)
{
	if (player1)
	{
		BounceAircraftOffWall(player1);
	}

	if (player2)
	{
		BounceAircraftOffWall(player2);
	}
}

void PhysicsSimulator::BounceAircraftOffWall(Aircraft* aircraft)
{
	const sf::Time kWallBounceGracePeriod = sf::milliseconds(250);
	const float bounce_force = 500.f;

	sf::Vector2f position = aircraft->getPosition();
	sf::Vector2f velocity = aircraft->GetVelocity();
	sf::FloatRect aircraft_bounds = aircraft->GetBoundingRect();

	bool bounced = false;

	// Check left boundary
	if (aircraft_bounds.position.x <= m_world_bounds.position.x)
	{
		position.x = m_world_bounds.position.x + (aircraft_bounds.size.x / 2.f);
		float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		velocity.x = -velocity.x;
		if (speed > 0.f)
		{
			velocity = (velocity / speed) * bounce_force;
		}
		else
		{
			velocity = sf::Vector2f(bounce_force, 0.f);
		}
		aircraft->SetVelocity(velocity);
		float angle = std::atan2(velocity.y, velocity.x) * 180.f / 3.14159f + 90.f;
		aircraft->setRotation(sf::degrees(angle));
		bounced = true;
	}
	// Check right boundary
	else if (aircraft_bounds.position.x + aircraft_bounds.size.x >= m_world_bounds.position.x + m_world_bounds.size.x)
	{
		position.x = m_world_bounds.position.x + m_world_bounds.size.x - (aircraft_bounds.size.x / 2.f);
		float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		velocity.x = -velocity.x;
		if (speed > 0.f)
		{
			velocity = (velocity / speed) * bounce_force;
		}
		else
		{
			velocity = sf::Vector2f(-bounce_force, 0.f);
		}
		aircraft->SetVelocity(velocity);
		float angle = std::atan2(velocity.y, velocity.x) * 180.f / 3.14159f + 90.f;
		aircraft->setRotation(sf::degrees(angle));
		bounced = true;
	}

	// Check top boundary
	if (aircraft_bounds.position.y <= m_world_bounds.position.y)
	{
		position.y = m_world_bounds.position.y + (aircraft_bounds.size.y / 2.f);
		float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		velocity.y = -velocity.y;
		if (speed > 0.f)
		{
			velocity = (velocity / speed) * bounce_force;
		}
		else
		{
			velocity = sf::Vector2f(0.f, bounce_force);
		}
		aircraft->SetVelocity(velocity);
		float angle = std::atan2(velocity.y, velocity.x) * 180.f / 3.14159f + 90.f;
		aircraft->setRotation(sf::degrees(angle));
		bounced = true;
	}
	// Check bottom boundary
	else if (aircraft_bounds.position.y + aircraft_bounds.size.y >= m_world_bounds.position.y + m_world_bounds.size.y)
	{
		position.y = m_world_bounds.position.y + m_world_bounds.size.y - (aircraft_bounds.size.y / 2.f);
		float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		velocity.y = -velocity.y;
		if (speed > 0.f)
		{
			velocity = (velocity / speed) * bounce_force;
		}
		else
		{
			velocity = sf::Vector2f(0.f, -bounce_force);
		}
		aircraft->SetVelocity(velocity);
		float angle = std::atan2(velocity.y, velocity.x) * 180.f / 3.14159f + 90.f;
		aircraft->setRotation(sf::degrees(angle));
		bounced = true;
	}

	if (bounced)
	{
		aircraft->setPosition(position);
		aircraft->SetCollisionImmunity(kWallBounceGracePeriod);
	}
}

void PhysicsSimulator::BounceProjectiles(CommandQueue& command_queue)
{
	// Apply bouncing to all projectiles in the scene
	Command projectileBouncer;
	projectileBouncer.category = static_cast<int>(ReceiverCategories::kProjectile);
	projectileBouncer.action = DerivedAction<SceneNode>([this](SceneNode& node, sf::Time)
		{
			BounceEntity(&node);
		});
	command_queue.Push(projectileBouncer);
}

void PhysicsSimulator::BounceEntity(SceneNode* entity)
{
	if (!entity)
		return;

	// Get entity bounds and velocity
	sf::FloatRect bounds = entity->GetBoundingRect();

	// Cast to Entity to access velocity methods
	Entity* moving_entity = dynamic_cast<Entity*>(entity);
	if (!moving_entity)
		return;

	sf::Vector2f velocity = moving_entity->GetVelocity();
	sf::Vector2f position = entity->getPosition();

	bool bounced = false;

	// Check collision with left and right walls
	if (bounds.position.x <= m_world_bounds.position.x)
	{
		// Hit left wall - bounce back right
		velocity.x = std::abs(velocity.x);
		position.x = m_world_bounds.position.x + (bounds.size.x / 2.f);
		bounced = true;
	}
	else if (bounds.position.x + bounds.size.x >= m_world_bounds.position.x + m_world_bounds.size.x)
	{
		// Hit right wall - bounce back left
		velocity.x = -std::abs(velocity.x);
		position.x = m_world_bounds.position.x + m_world_bounds.size.x - (bounds.size.x / 2.f);
		bounced = true;
	}

	// Check collision with top and bottom walls
	if (bounds.position.y <= m_world_bounds.position.y)
	{
		// Hit top wall - bounce back down
		velocity.y = std::abs(velocity.y);
		position.y = m_world_bounds.position.y + (bounds.size.y / 2.f);
		bounced = true;
	}
	else if (bounds.position.y + bounds.size.y >= m_world_bounds.position.y + m_world_bounds.size.y)
	{
		// Hit bottom wall - bounce back up
		velocity.y = -std::abs(velocity.y);
		position.y = m_world_bounds.position.y + m_world_bounds.size.y - (bounds.size.y / 2.f);
		bounced = true;
	}

	// Apply bounced velocity and position
	if (bounced)
	{
		moving_entity->SetVelocity(velocity);
		entity->setPosition(position);

		// Track bounces for projectiles and destroy if limit exceeded
		Projectile* projectile = dynamic_cast<Projectile*>(entity);
		if (projectile)
		{
			projectile->IncrementBounceCount();
			if (projectile->HasExceededBounceLimit())
			{
				projectile->Destroy();
			}
		}
	}
}
