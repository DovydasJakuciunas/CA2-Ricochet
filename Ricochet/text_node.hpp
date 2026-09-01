#pragma once
#include "scene_node.hpp"
#include "resource_identifiers.hpp"
#include <SFML/Graphics/Color.hpp>

class TextNode : public SceneNode
{
public:
	explicit TextNode(const FontHolder& fonts, std::string& text);
	void SetString(const std::string& text);
	void SetColor(const sf::Color& color);
	void SetCharacterSize(unsigned int size);
	void SetOutlineThickness(float thickness);
	void SetOutlineColor(const sf::Color& color);
	void SetBackgroundPadding(float padding);
	void SetBackgroundColor(const sf::Color& color);
	void SetBackgroundOutlineColor(const sf::Color& color);
	void SetBackgroundOutlineThickness(float thickness);

private:
	virtual void DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
	void UpdateBackgroundSize();

private:
	sf::Text m_text;
	mutable sf::RectangleShape m_background;
	float m_padding;
};

