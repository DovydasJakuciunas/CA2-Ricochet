#pragma once
#include "scene_node.hpp"
#include "network_protocol.hpp"

#include <queue>
#include <mutex>
class NetworkNode : public SceneNode
{
public:
	NetworkNode();
	void NotifyGameAction(GameActions::Type type, sf::Vector2f position);
	bool PollGameAction(GameActions::Action& out);
	virtual unsigned int GetCategory() const override;

private:
	std::queue<GameActions::Action> m_pending_actions;
	mutable std::mutex m_actions_mutex;	// Protects access to m_pending_actions
};