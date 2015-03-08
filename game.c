#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

void
game_init(game_t *game, uint64_t seed)
{
    game->seed = seed;
    game->time = 0;
    game->speed = 1;
    game->gold = INIT_GOLD;
    game->wood = INIT_WOOD;
    game->food = INIT_FOOD;
    game->population = INIT_POPULATION;
    game->map = map_generate(seed);
    game->map->high[MAP_WIDTH / 2][MAP_HEIGHT / 2].building = C_CASTLE;
}

bool
game_save(game_t *game, FILE *out)
{
    if (fwrite(game, sizeof(*game), 1, out) != 1)
        return false;
    if (fwrite(game->map->high, sizeof(game->map->high), 1, out) != 1)
        return false;
    return true;
}

bool
game_load(game_t *game, FILE *out)
{
    if (fread(game, sizeof(*game), 1, out) == 1) {
        game->map = map_generate(game->seed);
        if (fread(&game->map->high, sizeof(game->map->high), 1, out) == 1)
            return true;
    }
    return false;
}

void
game_free(game_t *game)
{
    map_free(game->map);
    game->map = NULL;
}

bool
game_build(game_t *game, enum building building, int x, int y)
{
    if (building == C_NONE) {
        /* Erase */
        if (game->map->high[x][y].building != C_NONE) {
            game->map->high[x][y].building = C_NONE;
            return true;
        }
    }
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT ||
        game->map->high[x][y].building != C_NONE) {
        return false;
    }

    bool valid = false;
    if (x > 0 && game->map->high[x - 1][y].building != C_NONE)
        valid = true;
    else if (y > 0 && game->map->high[x][y - 1].building != C_NONE)
        valid = true;
    else if (x + 1 < MAP_WIDTH && game->map->high[x + 1][y].building != C_NONE)
        valid = true;
    else if (y + 1 < MAP_HEIGHT && game->map->high[x][y + 1].building != C_NONE)
        valid = true;
    if (valid) {
        valid = false;
        enum map_base base = game->map->high[x][y].base;
        switch (building) {
        case C_NONE:
        case C_CASTLE:
        case C_ROAD:
            valid = strchr(" ~", base) == NULL;
            break;
        case C_LUMBERYARD:
            valid = strchr("#", base) != NULL;
            break;
        case C_STABLE:
            valid = strchr(".", base) != NULL;
            break;
        case C_HAMLET:
            valid = strchr(".#=", base) != NULL;
            if (valid)
                game->population += 100;
            break;
        case C_MINE:
            valid = strchr("=", base) != NULL;
            break;
            break;
        case C_FARM:
            valid = strchr(".#", base) != NULL;
            break;
        }
    }
    if (valid) {
        game->map->high[x][y].building = building;
        if (building == C_ROAD)
            game->map->high[x][y].building_age = 0;
        else
            game->map->high[x][y].building_age = INIT_BUILDING_AGE;
    }
    return valid;
}

void
game_date(game_t *game, char *buffer)
{
    long day = game->time / (60 * 24);
    long hour = ((game->time % (60 * 60)) / 60) % 24;
    long minute = game->time % 60;
    sprintf(buffer, "day %ld, %ld:%02ld", day, hour, minute);
}

#define DAY (60.0 * 24.0)

static void
yield_apply(game_t *game, yield_t yield)
{
    game->wood += yield.wood / DAY;
    game->food += yield.food / DAY;
    game->gold += yield.gold / DAY;
}

static void
building_process(game_t *game, enum building building)
{
    yield_t yield = {0, 0, 0};
    switch (building) {
    case C_NONE:
        return;
    case C_CASTLE:
        yield = YIELD_CASTLE;
        break;
    case C_LUMBERYARD:
        yield = YIELD_LUMBERYARD;
        break;
    case C_STABLE:
        yield = YIELD_STABLE;
        break;
    case C_HAMLET:
        yield = YIELD_HAMLET;
        break;
    case C_MINE:
        yield = YIELD_MINE;
        break;
    case C_ROAD:
        yield = YIELD_ROAD;
        break;
    case C_FARM:
        yield = YIELD_FARM;
        break;
    }
    yield_apply(game, yield);
}

void
yield_string(char *b, yield_t yield, bool rate)
{
    char *end = b;
    int count = 0;
    if (yield.gold)
        end += sprintf(end, "%s%d gold%s",
                       count++ > 0 ? ", " : "",
                       yield.gold,
                       rate ? "/d" : "");
    if (yield.wood)
        end += sprintf(end, "%s%d wood%s",
                       count++ > 0 ? ", " : "",
                       yield.wood,
                       rate ? "/d" : "");
    if (yield.food)
        end += sprintf(end, "%s%d food%s",
                       count++ > 0 ? ", " : "",
                       yield.food,
                       rate ? "/d" : "");
}

yield_t
game_step(game_t *game)
{
    struct {
        double gold;
        double food;
        double wood;
    } init = {game->gold, game->food, game->wood};
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            enum building building = game->map->high[x][y].building;
            if (building != C_NONE) {
                if (++game->map->high[x][y].building_age >= 0)
                    building_process(game, building);
            }
        }
    }
    game->time++;
    yield_t diff = {
        .gold = (game->gold - init.gold) * DAY,
        .food = (game->food - init.food) * DAY,
        .wood = (game->wood - init.wood) * DAY
    };
    return diff;
}

yield_t
building_cost(enum building building)
{
    switch (building) {
    case C_CASTLE:
        return COST_CASTLE;
    case C_LUMBERYARD:
        return COST_LUMBERYARD;
    case C_FARM:
        return COST_FARM;
    case C_STABLE:
        return COST_STABLE;
    case C_MINE:
        return COST_MINE;
    case C_ROAD:
        return COST_ROAD;
    case C_HAMLET:
        return COST_HAMLET;
    case C_NONE:
        break;
    }
    return (yield_t){0, 0, 0};
}

yield_t
building_yield(enum building building)
{
    switch (building) {
    case C_CASTLE:
        return YIELD_CASTLE;
    case C_LUMBERYARD:
        return YIELD_LUMBERYARD;
    case C_FARM:
        return YIELD_FARM;
    case C_STABLE:
        return YIELD_STABLE;
    case C_MINE:
        return YIELD_MINE;
    case C_ROAD:
        return YIELD_ROAD;
    case C_HAMLET:
        return YIELD_HAMLET;
    case C_NONE:
        break;
    }
    return (yield_t){0, 0, 0};
}
