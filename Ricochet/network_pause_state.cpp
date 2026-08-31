#include "network_pause_state.hpp"
#include "utility.hpp"

NetworkPauseState::NetworkPauseState(StateStack& stack, Context context) : State(stack, context), m_paused_text(context.fonts->Get(FontID::kMain)), m_instruction_text(context.fonts->Get(FontID::kMain))
{
	sf::Vector2f view_size = context.window->getView().getSize();

	m_paused_text.setString("Network Paused");
	m_paused_text.setCharacterSize(70);
	Utility::CentreOrigin(m_paused_text);
	m_paused_text.setPosition(sf::Vector2f(0.5f * view_size.x, 0.4f * view_size.y));

	m_instruction_text.setString("Waiting for network synchronization...");
	Utility::CentreOrigin(m_instruction_text);
	m_instruction_text.setPosition(sf::Vector2f(0.5f * view_size.x, 0.6f * view_size.y));
	GetContext().music->SetPaused(true);
}

NetworkPauseState::~NetworkPauseState()
{
	GetContext().music->SetPaused(false);
}

void NetworkPauseState::Draw()
{
	sf::RenderWindow& window = *GetContext().window;
	window.setView(window.getDefaultView());

	sf::RectangleShape backgroundShape;
	backgroundShape.setFillColor(sf::Color(0, 0, 0, 150));
	backgroundShape.setSize(window.getView().getSize());

	window.draw(backgroundShape);
	window.draw(m_paused_text);
	window.draw(m_instruction_text);
}

bool NetworkPauseState::Update(sf::Time dt)
{
	return false;
}

bool NetworkPauseState::HandleEvent(const sf::Event& event)
{
	const auto* key_pressed = event.getIf<sf::Event::KeyPressed>();
	if (!key_pressed)
	{
		return false;
	}
	if (key_pressed->scancode == sf::Keyboard::Scancode::Escape)
	{
		RequestStackPop();
	}
	if (key_pressed->scancode == sf::Keyboard::Scancode::Backspace)
	{
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
	}
	return false;
}
