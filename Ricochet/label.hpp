#pragma once
#include "component.hpp"
#include "resource_identifiers.hpp"

namespace gui
{
	class Label : public Component
	{
	public:
		typedef std::shared_ptr<Label> Ptr;

	public:
		Label(const std::string& text, const FontHolder& font);
		virtual bool IsSelectable() const override;
		void SetText(const std::string& text);
		void HandleEvent(const sf::Event& event) override;

	private:
		void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

	private:
		sf::Text m_text;
	};
}

