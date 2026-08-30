#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
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
	void HandlePlayerBoundaryCollision(const std::vector<Aircraft*>& players);

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
