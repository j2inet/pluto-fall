#include "game.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

using namespace pluto;

int main() {
    Game game;
    std::array<Input, max_devices> input{};
    input[0].start = true;
    game.update(0, input);
    assert(game.screen() == Screen::select);

    input = {};
    for (auto& device : input) device.join = true;
    game.update(0, input);
    for (const auto& player : game.players()) assert(player.joined);
    assert(game.players()[3].device == 3);

    input = {};
    input[0].right = true;
    game.update(0, input);
    assert(game.players()[0].model == 1);
    input = {};
    for (auto& device : input) device.fire_pressed = true;
    game.update(0, input);
    for (const auto& player : game.players()) assert(player.ready);
    input = {};
    input[1].bomb = true;
    game.update(0, input);
    assert(!game.players()[1].ready);
    input = {};
    input[0].start = true;
    game.update(0, input);
    assert(game.screen() == Screen::select);
    input = {};
    input[1].fire_pressed = true;
    game.update(0, input);
    input = {};
    input[0].start = true;
    game.update(0, input);
    assert(game.screen() == Screen::playing);

    input = {};
    input[0].fire = true;
    input[0].bomb = true;
    game.update(0.05f, input);
    assert(game.projectiles().size() == 2);
    assert(game.players()[0].bombs == 2);
    assert(game.players()[0].lives == 3 && game.players()[0].health == 10);
    assert(game.enemies().size() == 1);
    input = {};
    input[0].x = 1;
    for (int i = 0; i < 100; ++i) game.update(0.05f, input);
    assert(game.players()[0].x <= 0.97f);
    input = {};
    input[0].menu = true;
    game.update(0, input);
    assert(game.screen() == Screen::title);
    for (const auto& player : game.players()) assert(!player.joined);

    input = {};
    input[0].down = true;
    game.update(0, input);
    game.update(0, input);
    input = {};
    input[0].start = true;
    game.update(0, input);
    assert(game.take_website_request());
    assert(!game.take_website_request());

    Game controller_game;
    input = {};
    input[1].start = true;
    controller_game.update(0, input);
    input = {};
    input[1].join = true;
    controller_game.update(0, input);
    input = {};
    input[1].fire_pressed = true;
    controller_game.update(0, input);
    input = {};
    input[0].start = input[0].join = true;
    controller_game.update(0, input);
    assert(controller_game.screen() == Screen::playing);
    assert(controller_game.players()[0].device == 1);
    assert(!controller_game.players()[1].joined);
}
