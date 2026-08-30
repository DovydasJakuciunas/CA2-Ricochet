#include "settings_state.hpp"
#include "Utility.hpp"
#include "command.hpp"

SettingsState::SettingsState(StateStack& stack, Context context)
	: State(stack, context)
	, m_gui_container()
	, m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
	, m_current_binding_player(PlayerID::kPlayer1)
{
	// Player 1 Controls Header
	auto p1_label = std::make_shared<gui::Label>("PLAYER 1", *context.fonts);
	p1_label->setPosition(sf::Vector2f(80.f, 100.f));
	m_gui_container.Pack(p1_label);

	// Player 1 Controls
	AddButtonLabel(Action::kMoveUp, 150.f, "Move Up", context, PlayerID::kPlayer1);
	AddButtonLabel(Action::kMoveRight, 200.f, "Move Right", context, PlayerID::kPlayer1);
	AddButtonLabel(Action::kMoveLeft, 250.f, "Move Left", context, PlayerID::kPlayer1);
	AddButtonLabel(Action::kBulletFire, 300.f, "Fire", context, PlayerID::kPlayer1);
	AddButtonLabel(Action::kMissileFire, 350.f, "Missile Fire", context, PlayerID::kPlayer1);

	// Player 2 Controls Header
	auto p2_label = std::make_shared<gui::Label>("PLAYER 2", *context.fonts);
	p2_label->setPosition(sf::Vector2f(500.f, 100.f));
	m_gui_container.Pack(p2_label);

	// Player 2 Controls
	AddButtonLabel(Action::kMoveUp, 150.f, "Move Up", context, PlayerID::kPlayer2);
	AddButtonLabel(Action::kMoveRight, 200.f, "Move Right", context, PlayerID::kPlayer2);
	AddButtonLabel(Action::kMoveLeft, 250.f, "Move Left", context, PlayerID::kPlayer2);
	AddButtonLabel(Action::kBulletFire, 300.f, "Fire", context, PlayerID::kPlayer2);
	AddButtonLabel(Action::kMissileFire, 350.f, "Missile Fire", context, PlayerID::kPlayer2);

	UpdateLabels();

	auto back_button = std::make_shared<gui::Button>(context);
	back_button->setPosition(sf::Vector2f(80.f, 425.f));
	back_button->SetText("Back");
	back_button->SetCallback(std::bind(&SettingsState::RequestStackPop, this));
	m_gui_container.Pack(back_button);
}

void SettingsState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.draw(m_background_sprite);
    window.draw(m_gui_container);
}

bool SettingsState::Update(sf::Time dt)
{
    return true;
}

bool SettingsState::HandleEvent(const sf::Event& event)
{
    bool is_key_binding = false;

    // Check Player 1 buttons
    for (std::size_t action = 0; action < static_cast<int>(Action::kActionCount); ++action)
    {
        if (m_binding_buttons_p1[action]->IsActive())
        {
            is_key_binding = true;
            m_current_binding_player = PlayerID::kPlayer1;
            const auto* key_released = event.getIf<sf::Event::KeyReleased>();
            if (key_released)
            {
                GetContext().player->AssignKey(static_cast<Action>(action), key_released->scancode, PlayerID::kPlayer1);
                m_binding_buttons_p1[action]->Deactivate();
            }
            break;
        }
    }

    // Check Player 2 buttons
    if (!is_key_binding)
    {
        for (std::size_t action = 0; action < static_cast<int>(Action::kActionCount); ++action)
        {
            if (m_binding_buttons_p2[action]->IsActive())
            {
                is_key_binding = true;
                m_current_binding_player = PlayerID::kPlayer2;
                const auto* key_released = event.getIf<sf::Event::KeyReleased>();
                if (key_released)
                {
                    GetContext().player->AssignKey(static_cast<Action>(action), key_released->scancode, PlayerID::kPlayer2);
                    m_binding_buttons_p2[action]->Deactivate();
                }
                break;
            }
        }
    }

    if (is_key_binding)
    {
        UpdateLabels();
    }
    else
    {
        m_gui_container.HandleEvent(event);
    }
    return false;
}

void SettingsState::UpdateLabels()
{
    Player& player = *GetContext().player;

    // Update Player 1 labels
    for (std::size_t i = 0; i < static_cast<int>(Action::kActionCount); ++i)
    {
        sf::Keyboard::Scancode key = player.GetAssignedKey(static_cast<Action>(i), PlayerID::kPlayer1);
        m_binding_labels_p1[i]->SetText(Utility::toString(key));
    }

    // Update Player 2 labels
    for (std::size_t i = 0; i < static_cast<int>(Action::kActionCount); ++i)
    {
        sf::Keyboard::Scancode key = player.GetAssignedKey(static_cast<Action>(i), PlayerID::kPlayer2);
        m_binding_labels_p2[i]->SetText(Utility::toString(key));
    }
}

void SettingsState::AddButtonLabel(Action action, float y, const std::string& text, Context context, PlayerID player_id)
{
    int action_idx = static_cast<int>(action);

    if (player_id == PlayerID::kPlayer1)
    {
        m_binding_buttons_p1[action_idx] = std::make_shared<gui::Button>(context);
        m_binding_buttons_p1[action_idx]->setPosition(sf::Vector2f(80.f, y));
        m_binding_buttons_p1[action_idx]->SetText(text);
        m_binding_buttons_p1[action_idx]->SetToggle(true);

        m_binding_labels_p1[action_idx] = std::make_shared<gui::Label>("", *context.fonts);
        m_binding_labels_p1[action_idx]->setPosition(sf::Vector2f(300.f, y + 15.f));

        m_gui_container.Pack(m_binding_buttons_p1[action_idx]);
        m_gui_container.Pack(m_binding_labels_p1[action_idx]);
    }
    else
    {
        m_binding_buttons_p2[action_idx] = std::make_shared<gui::Button>(context);
        m_binding_buttons_p2[action_idx]->setPosition(sf::Vector2f(500.f, y));
        m_binding_buttons_p2[action_idx]->SetText(text);
        m_binding_buttons_p2[action_idx]->SetToggle(true);

        m_binding_labels_p2[action_idx] = std::make_shared<gui::Label>("", *context.fonts);
        m_binding_labels_p2[action_idx]->setPosition(sf::Vector2f(720.f, y + 15.f));

        m_gui_container.Pack(m_binding_buttons_p2[action_idx]);
        m_gui_container.Pack(m_binding_labels_p2[action_idx]);
    }
}
