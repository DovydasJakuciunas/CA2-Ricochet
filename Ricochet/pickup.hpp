#pragma once
#include "entity.hpp"
#include "pickup_type.hpp"
#include "resource_identifiers.hpp"
#include "aircraft.hpp"
#include <SFML/System/Time.hpp>

class Pickup : public Entity
{
public:
	Pickup(PickupType type, const TextureHolder& textures);
	virtual unsigned int GetCategory() const override;
	void Apply(Aircraft& player);
	virtual void DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
	virtual sf::FloatRect GetBoundingRect() const;
	virtual void UpdateCurrent(sf::Time dt, CommandQueue& commands) override;

private:
	PickupType m_type;
	sf::Sprite m_sprite;
	sf::Time m_lifetime;  // Track how long the pickup has existed
	static constexpr float kPickupLifetime = 12.f;  // Destroy after 12 seconds
};


