#pragma once
#include <SFML/Graphics.hpp>
#include "command_queue.hpp"

class Aircraft;
class SceneNode;

class PhysicsSimulator
{
public:
	PhysicsSimulator(const sf::FloatRect& world_bounds, const sf::View& camera);

	// Projectile and entity physics
	void BounceProjectiles(CommandQueue& command_queue);
	void BounceEntity(SceneNode* entity);

	// Player boundary collision handling
	void HandlePlayerBoundaryCollision(Aircraft* player1, Aircraft* player2);

	// View and bounds getters
	sf::FloatRect GetViewBounds() const;
	sf::FloatRect GetBattleFieldBounds() const;

private:
	sf::FloatRect m_world_bounds;
	sf::View m_camera;

	// Helper methods
	void BounceEntityInternal(SceneNode* entity);
	void BounceAircraftOffWall(Aircraft* aircraft);
};
