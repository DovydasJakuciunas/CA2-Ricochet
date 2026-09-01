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
								   CommandQueue& command_queue, SoundPlayer& sounds, bool is_host)
	: m_players(players)
	, m_scene_graph(scene_graph)
	, m_command_queue(command_queue)
	, m_sounds(sounds)
	, m_is_host(is_host)
	, m_pickup_collected_callback(nullptr)
{
}

void CollisionHandler::SetIsHost(bool is_host)
{
	m_is_host = is_host;
}

void CollisionHandler::SetPickupCollectedCallback(PickupCollectedCallback callback)
{
	m_pickup_collected_callback = callback;
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
			// Disabled: No player-to-player collisions
			continue;
		}
		// Pickup collection
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kPickup))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& pickup = static_cast<Pickup&>(*pair.second);

			// Only apply pickup effects on the host
			// Clients will receive the updated aircraft state through UpdateClientState() packets
			if (m_is_host)
			{
				pickup.Apply(aircraft);
			}

			// Notify that pickup was collected (before destroying)
			if (m_pickup_collected_callback)
			{
				m_pickup_collected_callback(pickup.GetID());
			}

			pickup.Destroy();
			aircraft.GetWeaponSystem().PlayLocalSound(m_command_queue, SoundEffect::kCollectPickup);
		}
		// Player hit by allied projectile (check owner for PvP)
		else if (MatchesCategories(pair, ReceiverCategories::kPlayerAircraft, ReceiverCategories::kAlliedProjectile))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			// In PvP, player can be damaged by other players' projectiles
			if (projectile.GetOwnerPlayerID() != player.GetPlayerID())
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
