#include "collision_handler.hpp"
#include "aircraft.hpp"
#include "projectile.hpp"
#include "pickup.hpp"
#include "receiver_categories.hpp"
#include "command_queue.hpp"
#include "sound_player.hpp"
#include "sound_effect.hpp"
#include "weapon_system.hpp"
#include <cmath>

CollisionHandler::CollisionHandler(std::vector<Aircraft*>& players, SceneNode& scene_graph,
								   CommandQueue& command_queue, SoundPlayer& sounds)
	: m_players(players)
	, m_scene_graph(scene_graph)
	, m_command_queue(command_queue)
	, m_sounds(sounds)
{
}

bool CollisionHandler::MatchesCategories(SceneNode::Pair& colliders, ReceiverCategories type1, ReceiverCategories type2) const
{
	unsigned int category1 = colliders.first->GetCategory();
	unsigned int category2 = colliders.second->GetCategory();

	if ((static_cast<int>(type1) & category1) && (static_cast<int>(type2) & category2))
	{
		return true;
	}
	else if ((static_cast<int>(type1) & category2) && (static_cast<int>(type2) & category1))
	{
		std::swap(colliders.first, colliders.second);
		return true;
	}
	else
	{
		return false;
	}
}

void CollisionHandler::HandleCollisions()
{
	std::set<SceneNode::Pair> collision_pairs;
	m_scene_graph.CheckSceneCollision(m_scene_graph, collision_pairs);

	for (SceneNode::Pair pair : collision_pairs)
	{
		// Player-to-Player collision
		if ((MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kPlayerAircraft)))
		{
			auto& aircraft1 = static_cast<Aircraft&>(*pair.first);
			auto& aircraft2 = static_cast<Aircraft&>(*pair.second);

			// Skip collision if either aircraft is immune
			if (aircraft1.IsCollisionImmune() || aircraft2.IsCollisionImmune())
			{
				continue;
			}

			// Calculate collision normal (direction from aircraft1 to aircraft2)
			sf::Vector2f collision_normal = aircraft2.GetWorldPosition() - aircraft1.GetWorldPosition();
			float distance = std::sqrt(collision_normal.x * collision_normal.x + collision_normal.y * collision_normal.y);
			if (distance > 0.f)
			{
				collision_normal /= distance;  // Normalize
			}
			else
			{
				collision_normal = sf::Vector2f(1.f, 0.f);  // Default direction if at same position
			}

			// Realistic bounce - reverse only the impact component
			// Decompose velocities into normal (collision) and tangential (parallel) components

			// Process aircraft1
			sf::Vector2f vel1 = aircraft1.GetVelocity();
			float vel1_normal = vel1.x * collision_normal.x + vel1.y * collision_normal.y;
			sf::Vector2f vel1_normal_vec = collision_normal * vel1_normal;
			sf::Vector2f vel1_tangential = vel1 - vel1_normal_vec;

			// Process aircraft2
			sf::Vector2f vel2 = aircraft2.GetVelocity();
			float vel2_normal = vel2.x * collision_normal.x + vel2.y * collision_normal.y;
			sf::Vector2f vel2_normal_vec = collision_normal * vel2_normal;
			sf::Vector2f vel2_tangential = vel2 - vel2_normal_vec;

			// Reverse normal components and recombine
			aircraft1.SetVelocity(-vel1_normal_vec + vel1_tangential);
			aircraft2.SetVelocity(-vel2_normal_vec + vel2_tangential);

			// Calculate speeds for damage logic
			float speed1 = std::sqrt(vel1.x * vel1.x + vel1.y * vel1.y);
			float speed2 = std::sqrt(vel2.x * vel2.x + vel2.y * vel2.y);

			// Grace period duration (0.5 seconds)
			constexpr sf::Time kCollisionGracePeriod = sf::milliseconds(500);

			// Only the slower aircraft takes damage
			if (speed1 < speed2)
			{
				aircraft1.Damage(10);
				aircraft1.SetCollisionImmunity(kCollisionGracePeriod);
			}
			else if (speed2 < speed1)
			{
				aircraft2.Damage(10);
				aircraft2.SetCollisionImmunity(kCollisionGracePeriod);
			}
			// If speeds are equal, both take damage and both get immunity
			else
			{
				aircraft1.Damage(10);
				aircraft2.Damage(10);
				aircraft1.SetCollisionImmunity(kCollisionGracePeriod);
				aircraft2.SetCollisionImmunity(kCollisionGracePeriod);
			}
		}
		// Pickup collection
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kPickup))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& pickup = static_cast<Pickup&>(*pair.second);
			//Collision response
			pickup.Apply(aircraft);
			pickup.Destroy();
			aircraft.GetWeaponSystem().PlayLocalSound(m_command_queue, SoundEffect::kCollectPickup);
		}
		// Player hit by allied projectile (check owner for PvP)
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kAlliedProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			// In PvP, player can be damaged by other players' projectiles
			if (projectile.GetOwnerPlayerID() != PlayerID::kPlayer1)
			{
				player.Damage(projectile.GetDamage());
				projectile.Destroy();
			}
		}
		// Player hit by enemy projectile
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kEnemyProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			//Collision response
			player.Damage(projectile.GetDamage());
			projectile.Destroy();
		}
	}
}
