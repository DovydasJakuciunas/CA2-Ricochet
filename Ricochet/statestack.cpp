#include "Statestack.hpp"

StateStack::PendingChange::PendingChange(StackActions action, StateID state_id) : action(action), state_id(state_id)
{
}

StateStack::StateStack(State::Context context) : m_context(context)
{
}

void StateStack::Update(sf::Time dt)
{
	for (auto itr = m_stack.rbegin(); itr != m_stack.rend(); ++itr)
	{
		if (!(*itr)->Update(dt))
		{
			break;
		}
	}
	ApplyPendingChanges();
}

void StateStack::Draw()
{
	for (State::Ptr& state : m_stack)
	{
		state->Draw();
	}

}

void StateStack::HandleEvent(const sf::Event& event)
{
	for (auto itr = m_stack.rbegin(); itr != m_stack.rend(); ++itr)
	{
		if (!(*itr)->HandleEvent(event))
		{
			break;
		}
	}
	ApplyPendingChanges();
}

void StateStack::PushState(StateID state_id)
{
	m_pending_list.emplace_back(PendingChange(StackActions::kPush, state_id));
}

void StateStack::PopState()
{
	m_pending_list.emplace_back(PendingChange(StackActions::kPop));
}

void StateStack::ClearStack()
{
	m_pending_list.emplace_back(PendingChange(StackActions::kClear));
}

void StateStack::ApplyPendingChanges()
{
	for (PendingChange change : m_pending_list)
	{
		switch (change.action)
		{
		case StackActions::kPush:
			m_stack.emplace_back(CreateState(change.state_id));
			break;
		case StackActions::kPop:
			m_stack.pop_back();
			break;
			//TODO should we clear the pending list when queueing up clear
		case StackActions::kClear:
			m_stack.clear();
			break;
		}
	}
	m_pending_list.clear();
}

bool StateStack::IsEmpty() const
{
	return m_stack.empty();
}

void StateStack::SetWorld(class World* world)
{
	m_context.world = world;
}

State::Ptr StateStack::CreateState(StateID state_id)
{
	auto found = m_state_factory.find(state_id);
	assert(found != m_state_factory.end());
	return found->second();
}
