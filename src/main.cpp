#include "game/game.h"

int main()
{
    using namespace minecraft;

    Game_Memory game_memory = {};

    game_memory.permanent_memory_size = MegaBytes(64);
    game_memory.permanent_memory      = malloc(game_memory.permanent_memory_size);

    if (!game_memory.permanent_memory)
    {
        fprintf(stderr, "failed to allocated %llu bytes of permanent memory\n", game_memory.permanent_memory_size);
        return -1;
    }

    game_memory.transient_memory_size = GigaBytes(4);
    game_memory.transient_memory      = malloc(game_memory.transient_memory_size);

    if (!game_memory.transient_memory)
    {
        fprintf(stderr, "failed to allocated %llu bytes of transient memory\n", game_memory.transient_memory_size);
        return -1;
    }

    game_memory.permanent_arena       = create_memory_arena(game_memory.permanent_memory,
                                                            game_memory.permanent_memory_size);

    game_memory.transient_arena       = create_memory_arena(game_memory.transient_memory,
                                                            game_memory.transient_memory_size);

    bool success = Game::initialize(&game_memory);

    if (!success)
    {
        fprintf(stderr, "[ERROR]: failed to initialize game");
        return -1;
    }

    Game::run();
    Game::shutdown();

    return 0;
}