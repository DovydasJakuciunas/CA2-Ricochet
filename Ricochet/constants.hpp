#pragma once
#include <SFML/Graphics/Color.hpp>

constexpr auto kTimePerFrame = 1.f / 60.f;
constexpr auto kMaxFireRate = 5;
constexpr auto kMaxSpread = 3;
constexpr auto kMissileRefill = 1;

constexpr auto kMaxPlayerHealth = 100;
constexpr auto kRotationSpeed = 2.5f;
constexpr auto kMaxBounces = 2;

// Player colors
constexpr sf::Color kPlayer1Color = sf::Color(100, 150, 255, 255);  // Blue
constexpr sf::Color kPlayer2Color = sf::Color(255, 100, 100, 255);  // Red