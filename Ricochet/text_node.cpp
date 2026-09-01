#include "text_node.hpp"
#include "utility.hpp"

TextNode::TextNode(const FontHolder& fonts, std::string& text)
	:m_text(fonts.Get(FontID::kMain), text, 20)
	, m_padding(0.f)
{
	Utility::CentreOrigin(m_text);
	m_background.setFillColor(sf::Color::Transparent);
}

void TextNode::SetString(const std::string& text)
{
	m_text.setString(text);
	Utility::CentreOrigin(m_text);
	UpdateBackgroundSize();
}

void TextNode::SetColor(const sf::Color& color)
{
	m_text.setFillColor(color);
}

void TextNode::SetCharacterSize(unsigned int size)
{
	m_text.setCharacterSize(size);
	Utility::CentreOrigin(m_text);
	UpdateBackgroundSize();
}

void TextNode::SetOutlineThickness(float thickness)
{
	m_text.setOutlineThickness(thickness);
	Utility::CentreOrigin(m_text);
}

void TextNode::SetOutlineColor(const sf::Color& color)
{
	m_text.setOutlineColor(color);
}

void TextNode::SetBackgroundPadding(float padding)
{
	m_padding = padding;
	UpdateBackgroundSize();
}

void TextNode::SetBackgroundColor(const sf::Color& color)
{
	m_background.setFillColor(color);
}

void TextNode::SetBackgroundOutlineColor(const sf::Color& color)
{
	m_background.setOutlineColor(color);
}

void TextNode::SetBackgroundOutlineThickness(float thickness)
{
	m_background.setOutlineThickness(thickness);
}

void TextNode::UpdateBackgroundSize()
{
	sf::FloatRect bounds = m_text.getLocalBounds();
	float width = bounds.size.x + (m_padding * 2);
	float height = bounds.size.y + (m_padding * 2);

	m_background.setSize(sf::Vector2f(width, height));

	// Center the background the same way as the text
	m_background.setPosition(sf::Vector2f(-width / 2.f, -height / 2.f));
}

void TextNode::DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
	// Draw background box first
	if (m_padding > 0.f)
	{
		target.draw(m_background, states);
	}
	// Draw text on top
	target.draw(m_text, states);
}

