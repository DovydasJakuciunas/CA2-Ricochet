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

CollisionHandler::CollisionHandler(Aircraft* player1, Aircraft* player2, SceneNode& scene_graph,
								   CommandQueue& command_queue, SoundPlayer& sounds)
	: m_player1(player1)
	, m_player2(player2)
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
		if ((MatchesCategories(pair, ReceiverCategories::kPlayer1Aircraft, ReceiverCategories::kPlayer2Aircraft)))
		{
			auto& player1 = static_cast<Aircraft&>(*pair.first);
			auto& player2 = static_cast<Aircraft&>(*pair.second);

			// Skip collision if either player is immune
			if (player1.IsCollisionImmune() || player2.IsCollisionImmune())
			{
				continue;
			}

			// Calculate velocity magnitudes
			sf::Vector2f vel1 = player1.GetVelocity();
			sf::Vector2f vel2 = player2.GetVelocity();
			float speed1 = std::sqrt(vel1.x * vel1.x + vel1.y * vel1.y);
			float speed2 = std::sqrt(vel2.x * vel2.x + vel2.y * vel2.y);

			// Calculate bounce direction (from player1 to player2)
			sf::Vector2f dir = player2.GetWorldPosition() - player1.GetWorldPosition();
			float distance = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (distance > 0.f)
			{
				dir /= distance;  // Normalize
			}
			else
			{
				dir = sf::Vector2f(1.f, 0.f);  // Default direction if at same position
			}

			// Bounce velocity magnitude
			constexpr float kBounceForce = 300.f;

			// Apply bounce velocities (push players apart)
			player1.SetVelocity(player1.GetVelocity() - dir * kBounceForce);
			player2.SetVelocity(player2.GetVelocity() + dir * kBounceForce);

			// Grace period duration (0.5 seconds)
			constexpr sf::Time kCollisionGracePeriod = sf::milliseconds(500);

			// Only the slower player takes damage
			if (speed1 < speed2)
			{
				player1.Damage(10);
				player1.SetCollisionImmunity(kCollisionGracePeriod);
			}
			else if (speed2 < speed1)
			{
				player2.Damage(10);
				player2.SetCollisionImmunity(kCollisionGracePeriod);
			}
			// If speeds are equal, both take damage and both get immunity
			else
			{
				player1.Damage(10);
				player2.Damage(10);
				player1.SetCollisionImmunity(kCollisionGracePeriod);
				player2.SetCollisionImmunity(kCollisionGracePeriod);
			}
		}
		// Legacy single-player collision handling
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kEnemyAircraft))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& enemy = static_cast<Aircraft&>(*pair.second);
			//Collision response
			player.Damage(enemy.GetHitPoints());
			enemy.Destroy();
		}
		// Pickup collection - Player 1
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer1Aircraft, ReceiverCategories::kPickup))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& pickup = static_cast<Pickup&>(*pair.second);
			//Collision response
			pickup.Apply(aircraft);
			pickup.Destroy();
			aircraft.GetWeaponSystem().PlayLocalSound(m_command_queue, SoundEffect::kCollectPickup);
		}
		// Pickup collection - Player 2
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer2Aircraft, ReceiverCategories::kPickup))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& pickup = static_cast<Pickup&>(*pair.second);
			//Collision response
			pickup.Apply(aircraft);
			pickup.Destroy();
			aircraft.GetWeaponSystem().PlayLocalSound(m_command_queue, SoundEffect::kCollectPickup);
		}
		// Player 1 hit by any projectile (check owner for PvP)
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer1Aircraft, ReceiverCategories::kAlliedProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			// In PvP, Player 1 can be damaged by Player 2's projectiles
			if (projectile.GetOwnerPlayerID() == PlayerID::kPlayer2)
			{
				player.Damage(projectile.GetDamage());
				projectile.Destroy();
			}
		}
		// Player 1 hit by enemy projectile
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer1Aircraft, ReceiverCategories::kEnemyProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			//Collision response
			player.Damage(projectile.GetDamage());
			projectile.Destroy();
		}
		// Player 2 hit by any projectile (check owner for PvP)
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer2Aircraft, ReceiverCategories::kAlliedProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			// In PvP, Player 2 can be damaged by Player 1's projectiles
			if (projectile.GetOwnerPlayerID() == PlayerID::kPlayer1)
			{
				player.Damage(projectile.GetDamage());
				projectile.Destroy();
			}
		}
		// Player 2 hit by enemy projectile
		else if (MatchesCategories(pair, ReceiverCategories::kPlayer2Aircraft, ReceiverCategories::kEnemyProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			//Collision response
			player.Damage(projectile.GetDamage());
			projectile.Destroy();
		}
		// Legacy enemy hit by player projectile
		else if (MatchesCategories(pair, ReceiverCategories::kEnemyAircraft, ReceiverCategories::kAlliedProjectile))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			//Collision response
			aircraft.Damage(projectile.GetDamage());
			projectile.Destroy();
		}
	}
}
