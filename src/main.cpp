#include "game/game.h"

int main()
{
    using namespace minecraft;

    bool success = Game::initialize();

    if (!success)
    {
        fprintf(stderr, "[ERROR]: failed to initialize game");
        return -1;
    }

    Game::run();
    Game::shutdown();

    return 0;
}