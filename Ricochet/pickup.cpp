#include "pickup.hpp"
#include "data_tables.hpp"
#include "utility.hpp"

namespace
{
    const std::vector<PickupData> Table = InitializePickupData();
}

Pickup::Pickup(PickupType type, const TextureHolder& textures)
    : Entity(1)
    , m_type(type)
    , m_sprite(textures.Get(Table[static_cast<int>(type)].m_texture), Table[static_cast<int>(type)].m_texture_rect)
    , m_lifetime(sf::Time::Zero)
{
    Utility::CentreOrigin(m_sprite);
}

unsigned int Pickup::GetCategory() const
{
    return static_cast<int>(ReceiverCategories::kPickup);
}

void Pickup::Apply(Aircraft& player)
{
    Table[static_cast<int>(m_type)].m_action(player);
}

void Pickup::DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_sprite, states);
}

sf::FloatRect Pickup::GetBoundingRect() const
{
    return GetWorldTransform().transformRect(m_sprite.getGlobalBounds());
}

void Pickup::UpdateCurrent(sf::Time dt, CommandQueue& commands)
{
    // Increment lifetime
    m_lifetime += dt;

    if (m_lifetime.asSeconds() >= kPickupLifetime)
    {
        Destroy();
    }

    Entity::UpdateCurrent(dt, commands);
}
