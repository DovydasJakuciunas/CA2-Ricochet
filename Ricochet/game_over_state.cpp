#include "game_over_state.hpp"
#include "utility.hpp"
#include "constants.hpp"
#include <string>

GameOverState::GameOverState(StateStack& stack, Context context)
    : State(stack, context)
    , m_game_over_text(context.fonts->Get(FontID::kMain))
    , m_elapsed_time(sf::Time::Zero)
    , m_winner_id(0)
{
    sf::Vector2f window_size(context.window->getSize());

    // Get all player kills from Player
    m_all_player_kills = context.player->GetAllPlayerKills();

    // Find the winner (player with most kills)
    int max_kills = -1;
    for (const auto& [playerID, kills] : m_all_player_kills)
    {
        if (kills > max_kills)
        {
            max_kills = kills;
            m_winner_id = playerID;
        }
    }

    // Set winner text
    m_game_over_text.setString("PLAYER " + std::to_string(m_winner_id + 1) + " HAS WON!");
    m_game_over_text.setCharacterSize(70);
    Utility::CentreOrigin(m_game_over_text);
    m_game_over_text.setPosition(sf::Vector2f(0.5 * window_size.x, 0.15 * window_size.y));

    // Create scoreboard texts for all players in a 2-column grid so up to 15
    // players fit on screen without overflowing.
    const int columns = 2;
    const float line_height = 45.f;
    const float column_width = 260.f;  // Horizontal spacing between the two columns
    const float top_margin = 0.15f * window_size.y + 80.f;  // Start below the winner text

    // Number of rows needed to fit all players across the columns
    const int player_count = static_cast<int>(m_all_player_kills.size());
    const int rows = (player_count + columns - 1) / columns;

    // X position of each column, centered as a group around the window centre
    const float first_column_x = 0.5f * window_size.x - column_width * (columns - 1) / 2.f;

    int index = 0;
    for (const auto& [playerID, kills] : m_all_player_kills)
    {
        sf::Text player_score(context.fonts->Get(FontID::kMain));
        player_score.setString("Player " + std::to_string(playerID + 1) + ": " + std::to_string(kills));
        player_score.setCharacterSize(30);

        // Color the text to match the player color
        sf::Color player_color = PlayerColors::GetColor(playerID);
        player_score.setFillColor(player_color);

        // Fill column by column: first fill all rows of column 0, then column 1
        int column = index / rows;
        int row = index % rows;

        Utility::CentreOrigin(player_score);
        player_score.setPosition(sf::Vector2f(
            first_column_x + column * column_width,
            top_margin + (row * line_height)));

        m_player_score_texts.push_back(player_score);
        index++;
    }
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
    for (const auto& player_score : m_player_score_texts)
    {
        window.draw(player_score);
    }
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
