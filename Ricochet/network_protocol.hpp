#pragma once
#include <SFML/System/Vector2.hpp>
const unsigned short SERVER_PORT = 50000; //Greater than 49151, in dynamic port range
namespace Server
{
	enum class PacketType
	{
		kBroadcastMessage, //Takes a std::string and sends it to all clients, they show on their screens for a number of seconds
		kInitialState, //takes two float values, the world height, and the initial scrolling in the world, then sf::Int32 with the number of aircraft, then for each aircraft its identifier, position, health and missiles
		kPlayerEvent, //This takes two sf::Int32 variables, the aircraft identifier and the action identifier from action.hpp, this is used to tell that a particular plane has triggered some action
		kPlayerRealtimeChange, //Same as playerevent for real time actions
		kPlayerConnect, //The same as SpawnSelf but indicates that an aircraft from a different client is connecting
		kPlayerDisconnect, //Takes sf::Int32 aircraft identifier that is disconnecting
		kSpawnPickup, //Similar to kSpawnEnemy. sf::Int32 for pickup type in PickupType.hpp and two floats for position
		kSpawnSelf, //This takes an sf::Int32 for the aircraft identifier and two float values for the initial position. 
		kUpdateClientState, //This sends aircraft identifier, X position, Y position, and rotation.
		kMissionSuccess // This has no arguments. It just informs the client that the game is over and the client can show the appropriate state
	};
}

namespace Client
{
	enum class PacketType
	{
		kPlayerEvent, // Two sf::Int32, aircraft identifer and event. It is used to request the server to trigger an event on the aircraft
		kPlayerRealtimeChange, // The same kPlayerEvent, additionally takes a boolean for real time action
		kInputCommand, // New: Aircraft identifier, input sequence, and input flags (bitfield)
		kStateUpdate, //sf::Int32 with number of local aircraft, for each aircraft send sf::Int32 identifier, two floats for position, health and ammo
		kHeartbeat, // Lightweight keepalive packet to prevent client timeout; no payload
		kGameEvent, //This is for explosions
		kQuit
	};
}

namespace GameActions
{
	enum Type
	{
		kEnemyExplode
	};

	struct Action
	{
		Action() = default;
		Action(Type type, sf::Vector2f position) :type(type), position(position)
		{

		}

		Type type;
		sf::Vector2f position;
	};
}

namespace InputCommand
{
	// Bit positions for input flags
	enum class InputFlag : uint8_t
	{
		kMoveLeft = 0,
		kMoveRight = 1,
		kMoveUp = 2,
		kBulletFire = 3,
		kMissileFire = 4
	};

	struct Command
	{
		uint8_t aircraft_identifier;
		uint8_t input_sequence;
		uint8_t input_flags;
	};
}