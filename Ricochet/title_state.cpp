#include "title_state.hpp"
#include "fontID.hpp"
#include "utility.hpp"

TitleState::TitleState(StateStack& stack, Context context) : State(stack, context), m_show_text(true), m_text_effect_time(sf::Time::Zero), m_background_sprite(context.textures->Get(TextureID::kTitleScreen)), m_text(context.fonts->Get(FontID::kMain))
{
    m_text.setString("Press any key to continue");
    Utility::CentreOrigin(m_text);
    m_text.setPosition(context.window->getView().getSize() / 2.f);
}

void TitleState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.draw(m_background_sprite);

    if (m_show_text)
    {
        window.draw(m_text);
    }
}

bool TitleState::Update(sf::Time dt)
{
    m_text_effect_time += dt;
    if (m_text_effect_time >= sf::seconds(0.5))
    {
        m_show_text = !m_show_text;
        m_text_effect_time = sf::Time::Zero;
    }
    return true;
}

bool TitleState::HandleEvent(const sf::Event& event)
{
    const auto* key_pressed = event.getIf<sf::Event::KeyPressed>();
    if (key_pressed)
    {
        RequestStackPop();
        RequestStackPush(StateID::kMenu);
    }
    return true;
}
