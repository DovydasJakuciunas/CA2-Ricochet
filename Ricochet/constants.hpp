#pragma once
#include <SFML/Graphics/Color.hpp>
#include <array>

constexpr auto kTimePerFrame = 1.f / 60.f;
constexpr auto kMaxFireRate = 5;
constexpr auto kMaxSpread = 3;
constexpr auto kMissileRefill = 1;

constexpr auto accelerationRate = 300.f;
constexpr auto boostedAccelerationRate = 15000.f;
constexpr auto boostThreshold = 2.f;

constexpr auto kMaxPlayerHealth = 100;
constexpr auto kRotationSpeed = 2.5f;
constexpr auto kMaxBounces = 2;

// PvP win condition - first player to reach this many kills wins
constexpr auto kKillsToWin = 3;

// Player colors - stored in array for cleaner organization
namespace PlayerColors
{
	constexpr std::array<sf::Color, 15> Colors = {{
		sf::Color(100, 150, 255, 255),   // Player 1 - Blue
		sf::Color(255, 100, 100, 255),   // Player 2 - Red
		sf::Color(100, 255, 100, 255),   // Player 3 - Green
		sf::Color(255, 255, 100, 255),   // Player 4 - Yellow
		sf::Color(255, 165, 0, 255),     // Player 5 - Orange
		sf::Color(200, 100, 255, 255),   // Player 6 - Purple
		sf::Color(100, 255, 255, 255),   // Player 7 - Cyan
		sf::Color(255, 192, 203, 255),   // Player 8 - Pink
		sf::Color(50, 255, 50, 255),     // Player 9 - Lime
		sf::Color(255, 215, 0, 255),     // Player 10 - Gold
		sf::Color(0, 206, 209, 255),     // Player 11 - Dark Turquoise
		sf::Color(255, 0, 255, 255),     // Player 12 - Magenta
		sf::Color(205, 133, 63, 255),    // Player 13 - Peru/Brown
		sf::Color(192, 192, 192, 255),   // Player 14 - Silver
		sf::Color(34, 139, 34, 255)      // Player 15 - Forest Green
	}};

	// Get color by player index (0-14)
	constexpr sf::Color GetColor(unsigned int playerIndex)
	{
		return playerIndex < Colors.size() ? Colors[playerIndex] : Colors[0];
	}
}