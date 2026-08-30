#pragma once
#include <SFML/System/Clock.hpp>
#include "stack_actions.hpp"
#include <SFML/Window/Event.hpp>
#include <map>
#include <functional>
#include "stateid.hpp"
#include "state.hpp"

class StateStack
{
public:
	explicit StateStack(State::Context context);
	template<typename T>
	void RegisterState(StateID state_id);
	void Update(sf::Time dt);
	void Draw();
	void HandleEvent(const sf::Event& event);

	void PushState(StateID state_id);
	void PopState();
	void ClearStack();
	bool IsEmpty() const;

	void SetWorld(class World* world);

private:
	State::Ptr CreateState(StateID state_id);
	void ApplyPendingChanges();

private:
	struct PendingChange
	{
		explicit PendingChange(StackActions action, StateID state_id = StateID::kNone);
		StackActions action;
		StateID state_id;
	};

private:
	//TODO is vector the right data structure here - list?
	std::vector<State::Ptr> m_stack;
	std::vector<PendingChange> m_pending_list;
	State::Context m_context;
	std::map<StateID, std::function<State::Ptr()>> m_state_factory;
};

// Helper template to detect if type has Initialize method
template<typename T, typename = void>
struct has_initialize : std::false_type {};

template<typename T>
struct has_initialize<T, std::void_t<decltype(std::declval<T>().Initialize())>> : std::true_type {};

template<typename T>
void StateStack::RegisterState(StateID state_id)
{
	m_state_factory[state_id] = [this]()
	{
		auto state = std::make_shared<T>(*this, m_context);

		// Call Initialize if T supports it (for enable_shared_from_this)
		if constexpr (has_initialize<T>::value)
		{
			state->Initialize();
		}

		return state;
	};
}

