#include "game.h"

namespace minecraft {

    void Game::shutdown() {
        internal_data.is_running = false;
    }

    Game_Data Game::internal_data;
}