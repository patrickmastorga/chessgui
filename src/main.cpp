#include <SFML/Graphics.hpp>
#include "DrawableBoard.hpp"

int main()
{
    sf::RenderWindow window(sf::VideoMode(960, 960), "chessgui", sf::Style::Close | sf::Style::Titlebar);
    auto desktop = sf::VideoMode::getDesktopMode();
    window.setPosition(sf::Vector2i(desktop.width/2 - window.getSize().x/2, desktop.height/2 - window.getSize().y/2 - 50));
    DrawableBoard board(sf::Vector2f(0, 0), true);

    bool mouseHold = false;
    bool validPeiceSelected = false;

    while (window.isOpen())
    {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
                case sf::Event::Closed:
                    window.close();
            }
        }

        // Mouse Input
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            if (mouseHold) {
                // Mouse is being held
                board.mouseDrag(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
            } else {
                // Mousedown
                board.mouseDown(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
                mouseHold = true;
            }
        
        } else if (mouseHold) {
            // Mouseup
            board.mouseUp(sf::Vector2f((float)mousePos.x, (float)mousePos.y));
            mouseHold = false;
        }
        

        window.clear();
        window.draw(board);
        window.display();
    }
    
    return 0;
}