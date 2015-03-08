#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "display.h"
#include "map.h"
#include "game.h"

#define FPS 6
#define PERIOD (1000000 / 6)
#define SPEED_MAX 256
#define SPEED_FACTOR 4
#define PERSIST_FILE "persist.gcom"

#define FONT_KEY (font_t){COLOR_RED, COLOR_BLACK, true}

static bool
is_exit_key(int key)
{
    return key == 'q' || key == 27 || key == 3;
}

static int
game_getch(game_t *game, panel_t *world)
{
    for (;;) {
        map_draw(game->map, world);
        display_refresh();
        uint64_t wait = device_uepoch() % PERIOD;
        if (device_kbhit(wait))
            return device_getch();
    }
}

static void
popup_error(char *format, ...)
{
    font_t font = {COLOR_YELLOW, COLOR_BLACK, true};
    va_list ap;
    va_start(ap, format);
    int length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    va_start(ap, format);
    char buffer[length + 1];
    vsprintf(buffer, format, ap);
    va_end(ap);
    int width = length + 2;
    int height = 3;
    panel_t popup;
    panel_center_init(&popup, width, height);
    panel_puts(&popup, 1, 1, font, buffer);
    display_push(&popup);
    for (;;) {
        int key = display_getch();
        if (is_exit_key(key) || key == 13 || key == ' ')
            break;
    }
    display_pop();
    panel_free(&popup);
    display_refresh();
}

static bool
popup_quit(bool saving)
{
    panel_t popup;
    char *message;
    if (saving)
        message = "Really quit? (y/n)";
    else
        message = "Really quit without saving? (y/n)";
    size_t length = strlen(message);
    panel_center_init(&popup, length + 2, 3);
    panel_puts(&popup, 1, 1, FONT_DEFAULT, message);
    panel_attr(&popup, length - 3, 1, FONT_KEY);
    panel_attr(&popup, length - 1, 1, FONT_KEY);
    font_t alert = {COLOR_YELLOW, COLOR_BLACK, true};
    if (!saving)
        for (int i = 0; i < 14; i++)
            panel_attr(&popup, i + 13, 1, alert);
    display_push(&popup);
    display_refresh();
    int input = device_getch();
    display_pop();
    panel_free(&popup);
    display_refresh();
    if (input == 'y' || input == 'Y')
        return true;
    return false;
}

static void
popup_unknown_key(int key)
{
    popup_error("Unknown input: %d", key);
}

#define SIDEMENU_WIDTH (DISPLAY_WIDTH - MAP_WIDTH)

static void
sidemenu_draw(panel_t *p, game_t *game, yield_t diff)
{
    font_t font_title = {COLOR_WHITE, COLOR_BLACK, false};
    panel_fill(p, font_title, ' ');
    panel_border(p, font_title);
    panel_puts(p, 5, 1, font_title, "Goblin-COM");

    font_t font_totals = {COLOR_WHITE, COLOR_BLACK, true};
    int ty = 3;
    panel_printf(p, 2, ty++, font_totals, "Gold: %ld%+d",
                 (long)game->gold, (int)diff.gold);
    panel_printf(p, 2, ty++, font_totals, "Food: %ld%+d",
                 (long)game->food, (int)diff.food);
    panel_printf(p, 2, ty++, font_totals, "Wood: %ld%+d",
                 (long)game->wood, (int)diff.wood);
    panel_printf(p, 2, ty++, font_totals, "Pop.: %ld", (long)game->population);
    for (int y = 3; y < 6; y++) {
        bool seen = false;
        for (int x = 7; x < p->w - 1; x++) {
            if (strchr("+-", panel_getc(p, x, y)) != NULL)
                seen = true;
            if (seen)
                panel_attr(p, x, y, font_title);
        }
    }

    font_t base = {COLOR_WHITE, COLOR_BLACK, false};
    int x = 2;
    int y = 8;
    panel_puts(p, x,   y+0, base, "Create Building");
    panel_attr(p, x+7, y+0, FONT_KEY);
    panel_puts(p, x,   y+1, base, "View Heroes");
    panel_attr(p, x+5, y+1, FONT_KEY);
    panel_puts(p, x,   y+2, base, "Show Visibility");
    panel_attr(p, x+5, y+2, FONT_KEY);

    char date[128];
    game_date(game, date);
    panel_puts(p, 2, 20, font_totals, date);

    panel_puts(p, 2, 21, base, "Speed: ");
    for (int x = 0, i = 1; i <= game->speed; i *= SPEED_FACTOR, x++)
        panel_puts(p, 9 + x, 21, font_totals, ">");
}

static char
popup_build_select(game_t *game, panel_t *world)
{
    int width = 56;
    int height = 20;
    panel_t build;
    panel_center_init(&build, width, height);
    display_push(&build);
    font_t border = {COLOR_WHITE, COLOR_BLACK, false};
    panel_border(&build, border);

    int input;
    char result = 0;
    panel_t *p = &build;
    font_t item = {COLOR_WHITE, COLOR_BLACK, true};
    font_t desc = {COLOR_WHITE, COLOR_BLACK, false};
    char cost[128];
    char yield[128];

    int y = 1;
    yield_string(cost, COST_LUMBERYARD, false);
    yield_string(yield, YIELD_LUMBERYARD, true);
    panel_printf(p,   1, y++, item, "(w) Lumberyard [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc, "Target: forest (%c)", BASE_FOREST);
    panel_attr(p, 2, y - 3, FONT_KEY);

    yield_string(cost, COST_FARM, false);
    yield_string(yield, YIELD_FARM, true);
    panel_printf(p, 1, y++, item, "(f) Farm [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc, "Target: grassland (%c), forest (%c)",
                 BASE_GRASSLAND, BASE_FOREST);
    panel_attr(p, 2, y - 3, FONT_KEY);

    yield_string(cost, COST_STABLE, false);
    yield_string(yield, YIELD_STABLE, true);
    panel_printf(p, 1, y++, item, "(s) Stable [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc, "Target: grassland (%c)", BASE_GRASSLAND);
    panel_attr(p, 2, y - 3, FONT_KEY);

    yield_string(cost, COST_MINE, false);
    yield_string(yield, YIELD_MINE, true);
    panel_printf(p, 1, y++, item, "(m) Mine [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc, "Target: hill (%c)", BASE_HILL);
    panel_attr(p, 2, y - 3, FONT_KEY);

    yield_string(cost, COST_HAMLET, false);
    yield_string(yield, YIELD_HAMLET, true);
    panel_printf(p, 1, y++, item, "(h) Hamlet [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc,
                 "Target: grassland (%c), forest (%c), hill (%c)",
                 BASE_GRASSLAND, BASE_FOREST, BASE_HILL);
    panel_attr(p, 2, y - 3, FONT_KEY);

    yield_string(cost, YIELD_ROAD, false);
    yield_string(yield, YIELD_ROAD, true);
    panel_printf(p, 1, y++, item, "(r) Road [%s]", cost);
    panel_printf(p, 5, y++, desc, "Yield: %s", yield);
    panel_printf(p, 5, y++, desc, "Target: (any)");
    panel_attr(p, 2, y - 3, FONT_KEY);

    while (result == 0 && !is_exit_key(input = game_getch(game, world)))
        if (strchr("wfshmr", input))
            result = toupper(input);
    if (result == 'R')
        result = '+';
    display_pop();
    panel_free(&build);
    return result;
}

static bool
arrow_adjust(int input, int *x, int *y)
{
    switch (input) {
    case ARROW_U:
        (*y)--;
        return true;
    case ARROW_D:
        (*y)++;
        return true;
    case ARROW_L:
        (*x)--;
        return true;
    case ARROW_R:
        (*x)++;
        return true;
    case ARROW_UL:
        (*x)--;
        (*y)--;
        return true;
    case ARROW_UR:
        (*x)++;
        (*y)--;
        return true;
    case ARROW_DL:
        (*x)--;
        (*y)++;
        return true;
    case ARROW_DR:
        (*x)++;
        (*y)++;
        return true;
    }
    return false;
}

static bool
select_position(game_t *game, panel_t *world, int *x, int *y)
{
    font_t highlight = {COLOR_WHITE, COLOR_RED, true};
    *x = MAP_WIDTH / 2;
    *y = MAP_HEIGHT / 2;
    bool selected = false;
    panel_t overlay;
    panel_init(&overlay, 0, 0, MAP_WIDTH, MAP_HEIGHT);
    panel_putc(&overlay, *x, *y, highlight, panel_getc(world, *x, *y));
    display_push(&overlay);
    int input;
    while (!selected && !is_exit_key(input = game_getch(game, world))) {
        panel_erase(&overlay, *x, *y);
        arrow_adjust(input, x, y);
        panel_putc(&overlay, *x, *y, highlight, panel_getc(world, *x, *y));
        if (input == 13)
            selected = true;
    }
    display_pop();
    panel_free(&overlay);
    return selected;
}

static bool
spend(game_t *game, yield_t yield)
{
    if (game->food >= yield.food &&
        game->wood >= yield.wood &&
        game->gold >= yield.gold) {
        game->food -= yield.food;
        game->wood -= yield.wood;
        game->gold -= yield.gold;
        return true;
    }
    return false;
}

int
main(void)
{
    int w, h;
    device_size(&w, &h);
    if (w < DISPLAY_WIDTH || h < DISPLAY_HEIGHT) {
        printf("Goblin-COM requires a terminal of at least %dx%d characters!\n"
               "Press enter to exit ...", DISPLAY_WIDTH, DISPLAY_HEIGHT);
        getchar();
        exit(EXIT_FAILURE);
    }
    display_init();
    device_title("Goblin-COM");

    game_t game;
    FILE *save = fopen(PERSIST_FILE, "rb");
    if (save) {
        game_load(&game, save);
        fclose(save);
        unlink(PERSIST_FILE);
    } else {
        game_init(&game, device_uepoch());
    }

    panel_t world;
    panel_init(&world, 0, 0, MAP_WIDTH, MAP_HEIGHT);
    display_push(&world);

    panel_t sidemenu;
    panel_init(&sidemenu, DISPLAY_WIDTH - SIDEMENU_WIDTH, 0,
               SIDEMENU_WIDTH, DISPLAY_HEIGHT);
    display_push(&sidemenu);

    /* Main Loop */
    bool running = true;
    while (running) {
        yield_t diff;
        for (int i = 0; i < game.speed; i++)
            diff = game_step(&game);
        sidemenu_draw(&sidemenu, &game, diff);
        map_draw(game.map, &world);
        display_refresh();
        uint64_t wait = device_uepoch() % PERIOD;
        if (device_kbhit(wait)) {
            int key = device_getch();
            switch (key) {
            case 'b': {
                char building = popup_build_select(&game, &world);
                if (building) {
                    yield_t cost = building_cost(building);
                    if (!spend(&game, cost)) {
                        popup_error("Not enough funding/materials!");
                    } else {
                        int x, y;
                        if (select_position(&game, &world, &x, &y))
                            if (!game_build(&game, building, x, y))
                                popup_error("Invalid building location!");
                    }
                }
            } break;
            case '>':
                if (game.speed == 0)
                    game.speed = 1;
                else if (game.speed < SPEED_MAX)
                    game.speed *= SPEED_FACTOR;
                break;
            case '<':
                game.speed /= SPEED_FACTOR;
                break;
            case 'q':
                running = !popup_quit(true);
                FILE *save = fopen(PERSIST_FILE, "wb");
                game_save(&game, save);
                fclose(save);
                break;
            case 'Q':
                running = !popup_quit(false);
                break;
            default:
                popup_unknown_key(key);
                break;
            }
        }
    };

    game_free(&game);

    display_pop(); // info
    display_pop(); // world
    panel_free(&sidemenu);
    panel_free(&world);
    display_free();
    return 0;
}
