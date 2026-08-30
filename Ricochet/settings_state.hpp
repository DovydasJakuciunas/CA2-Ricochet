#pragma once
#include "statestack.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include "container.hpp"
#include "button.hpp"
#include "label.hpp"
#include "world.hpp"

class SettingsState : public State, public std::enable_shared_from_this<SettingsState>
{
public:
	SettingsState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event) override;

private:
	void UpdateLabels();
	void AddButtonLabel(Action action, float y, const std::string& text, Context context, PlayerID player_id = PlayerID::kPlayer1);

private:
	sf::Sprite m_background_sprite;
	gui::Container m_gui_container;
	std::array<gui::Button::Ptr, static_cast<int>(Action::kActionCount)> m_binding_buttons_p1;
	std::array<gui::Label::Ptr, static_cast<int>(Action::kActionCount)> m_binding_labels_p1;
	std::array<gui::Button::Ptr, static_cast<int>(Action::kActionCount)> m_binding_buttons_p2;
	std::array<gui::Label::Ptr, static_cast<int>(Action::kActionCount)> m_binding_labels_p2;

	PlayerID m_current_binding_player;

	gui::Button::Ptr m_back_button;
};
