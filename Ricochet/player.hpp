#pragma once
#include "command_queue.hpp"
#include <SFML/Window/Event.hpp>
#include "action.hpp"
#include <map>
#include "command.hpp"
#include "mission_status.hpp"
#include "constants.hpp"
#include "utility.hpp"
#include <cmath>
#include <SFML/Network/TcpSocket.hpp>
#include "key_binding.hpp"
#include "network_protocol.hpp"
#include <SFML/Network/Packet.hpp>

// Forward declaration
class GameplayManager;

class Player
{
public:
	Player();
	Player(sf::TcpSocket* socket, uint8_t identifier, const KeyBinding* binding);
	void HandleEvent(const sf::Event& event, CommandQueue& command_queue);
	void HandleRealTimeInput(CommandQueue& command_queue);
	void HandleRealtimeNetworkInput(CommandQueue& commands);

	void AssignKey(Action action, sf::Keyboard::Scancode key);
	sf::Keyboard::Scancode GetAssignedKey(Action action) const;
	void SetMissionStatus(MissionStatus status);
	MissionStatus GetMissionStatus() const;

	// PvP score tracking - Gets from GameplayManager
	void SetGameplayManager(GameplayManager* gameplayMgr);
	int GetPlayerKills(uint8_t playerID) const;
	const std::map<uint8_t, int>& GetAllPlayerKills() const;

	//Multiplayer Additions
	void DisableAllRealtimeActions(bool enable);
	void HandleNetworkEvent(Action action, CommandQueue& commands);
	void HandleNetworkRealtimeChange(Action action, bool action_enabled);
	bool IsLocal() const;
	void SetKeyBinding(const KeyBinding* binding);
	const std::map<Action, bool>& GetActionProxies() const;

private:
	void InitialiseActions();
	static bool IsRealTimeAction(Action action);

private:
	MissionStatus m_current_mission_status;
	bool m_was_forward_pressed;

	// Reference to GameplayManager for kill tracking
	GameplayManager* m_gameplay_manager;

	//Multiplayer Additions
private:
	const KeyBinding* m_key_binding;
	std::map<Action, Command> m_action_binding;
	std::map<Action, bool> m_action_proxies;
	uint8_t m_identifier;
	sf::TcpSocket* m_socket;
};

