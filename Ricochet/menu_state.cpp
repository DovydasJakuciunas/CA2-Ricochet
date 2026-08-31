#include "menu_state.hpp"
#include "fontID.hpp"
#include <SFML/Graphics/Text.hpp>
#include "utility.hpp"
#include "menu_options.hpp"
#include "button.hpp"
#include <iostream>

MenuState::MenuState(StateStack& stack, Context context) : State(stack, context), m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
{
    auto host_button = std::make_shared<gui::Button>(context);
    host_button->setPosition(sf::Vector2f(100, 250));
    host_button->SetText("Host Game");
    host_button->SetCallback([this]()
        {
            std::cout << "[MENU] Host Game button clicked - starting as server..." << std::endl;
            RequestStackPop();
            RequestStackPush(StateID::kHostGame);
        });

    auto join_button = std::make_shared<gui::Button>(context);
    join_button->setPosition(sf::Vector2f(100, 300));
    join_button->SetText("Join Game");
    join_button->SetCallback([this]()
        {
            std::cout << "[MENU] Join Game button clicked - connecting as client..." << std::endl;
            RequestStackPop();
            RequestStackPush(StateID::kJoinGame);
        });

    auto settings_button = std::make_shared<gui::Button>(context);
    settings_button->setPosition(sf::Vector2f(100, 350));
    settings_button->SetText("Settings");
    settings_button->SetCallback([this]()
        {
            RequestStackPush(StateID::kSettings);
        });

    auto exit_button = std::make_shared<gui::Button>(context);
    exit_button->setPosition(sf::Vector2f(100, 400));
    exit_button->SetText("Exit");
    exit_button->SetCallback([this]()
        {
            RequestStackPop();
        });

    m_gui_container.Pack(host_button);
    m_gui_container.Pack(join_button);
    m_gui_container.Pack(settings_button);
    m_gui_container.Pack(exit_button);

    context.music->Play(MusicThemes::kMenuTheme);
}

void MenuState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.setView(window.getDefaultView());
    window.draw(m_background_sprite);
    window.draw(m_gui_container);
}

bool MenuState::Update(sf::Time dt)
{
    return true;
}

bool MenuState::HandleEvent(const sf::Event& event)
{
    m_gui_container.HandleEvent(event);
    return true;
}

