#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace pluto {

constexpr std::size_t max_players = 4;
constexpr std::size_t max_devices = 5; // Keyboard followed by four XInput controllers.

enum class Screen { title, select, playing, options, credits };

struct Input {
    float x = 0;
    float y = 0;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool join = false;
    bool fire = false;
    bool fire_pressed = false;
    bool bomb = false;
    bool menu = false;
    bool start = false;
};

struct Player {
    bool joined = false;
    bool ready = false;
    int device = -1;
    int model = 0;
    int lives = 3;
    int health = 10;
    int bombs = 3;
    float x = 0.5f;
    float y = 0.85f;
    float fire_cooldown = 0;
};

struct Projectile {
    float x, y;
    bool bomb;
};

struct Enemy {
    float x, y;
    int health;
};

class Game {
public:
    void update(float elapsed, const std::array<Input, max_devices>& input);
    Screen screen() const { return screen_; }
    int menu_selection() const { return menu_selection_; }
    float effects_volume() const { return effects_volume_; }
    float music_volume() const { return music_volume_; }
    const std::array<Player, max_players>& players() const { return players_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    bool take_website_request();

private:
    void begin_game();
    void return_to_title();
    Screen screen_ = Screen::title;
    int menu_selection_ = 0;
    float effects_volume_ = 0.8f;
    float music_volume_ = 0.8f;
    bool website_request_ = false;
    float spawn_timer_ = 0;
    unsigned spawn_index_ = 0;
    std::array<Player, max_players> players_{};
    std::vector<Projectile> projectiles_;
    std::vector<Enemy> enemies_;
};

}
