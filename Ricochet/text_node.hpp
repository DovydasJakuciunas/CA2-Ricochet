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

private:
	virtual void DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;

private:
	sf::Text m_text;
};

