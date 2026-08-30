#include "state.hpp"
#include "statestack.hpp"

State::State(StateStack& stack, Context context) : m_stack(&stack), m_context(context)
{
}

State::~State()
{
}

State::Context::Context(sf::RenderWindow& window, TextureHolder& textures, FontHolder& fonts, Player& player, MusicPlayer& music, SoundPlayer& sound, class World* world) : window(&window), textures(&textures), fonts(&fonts), player(&player), music(&music), sound(&sound), world(world)
{
}

void State::RequestStackPush(StateID state_id)
{
	m_stack->PushState(state_id);
}

void State::RequestStackPop()
{
	m_stack->PopState();
}

void State::RequestStackClear()
{
	m_stack->ClearStack();
}

State::Context State::GetContext() const
{
	return m_context;
}

StateStack* State::GetStack() const
{
	return m_stack;
}
