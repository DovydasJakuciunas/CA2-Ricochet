#include "game_over_state.hpp"
#include "utility.hpp"
#include "constants.hpp"
#include <string>

GameOverState::GameOverState(StateStack& stack, Context context)
    : State(stack, context)
    , m_game_over_text(context.fonts->Get(FontID::kMain))
    , m_score_text(context.fonts->Get(FontID::kMain))
    , m_elapsed_time(sf::Time::Zero)
    , m_is_pvp_win(true)
    , m_player1_kills(0)
    , m_player2_kills(0)
{
    sf::Vector2f window_size(context.window->getSize());

    if (context.player->GetMissionStatus() == MissionStatus::kMissionSuccess)
    {
        m_game_over_text.setString("PLAYER 1 HAS WON");
    }
    else
    {
        m_game_over_text.setString("PLAYER 2 HAS WON");
    }

    m_game_over_text.setCharacterSize(70);
    Utility::CentreOrigin(m_game_over_text);
    m_game_over_text.setPosition(sf::Vector2f(0.5 * window_size.x, 0.4 * window_size.y));

    // Initialize score text from Player context
    m_player1_kills = context.player->GetPlayer1Kills();
    m_player2_kills = context.player->GetPlayer2Kills();

    m_score_text.setCharacterSize(50);
    m_score_text.setFillColor(sf::Color::White);
    m_score_text.setString(std::to_string(m_player1_kills) + " - " + std::to_string(m_player2_kills));
    Utility::CentreOrigin(m_score_text);
    m_score_text.setPosition(sf::Vector2f(0.5 * window_size.x, 0.55 * window_size.y));
}

void GameOverState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.setView(window.getDefaultView());

    //Create a dark semi-transparent background
    sf::RectangleShape background_shape;
    background_shape.setFillColor(sf::Color(0, 0, 0, 150));
    background_shape.setSize(window.getView().getSize());

    window.draw(background_shape);
    window.draw(m_game_over_text);
    window.draw(m_score_text);
}

bool GameOverState::Update(sf::Time dt)
{
    //Show gameover for 3 seconds and then return to the main menu
    m_elapsed_time += dt;
    if (m_elapsed_time >= sf::seconds(3.f))
    {
        RequestStackClear();
        RequestStackPush(StateID::kMenu);
    }
    return false;
}

bool GameOverState::HandleEvent(const sf::Event& event)
{
    return false;
}
