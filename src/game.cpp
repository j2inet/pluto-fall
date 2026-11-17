#include "game.h"

#include <algorithm>
#include <cmath>

namespace pluto {
namespace {
bool close(float x1, float y1, float x2, float y2, float radius) {
    const float dx = (x1 - x2) * (1280.0f / 720.0f), dy = y1 - y2;
    return dx * dx + dy * dy <= radius * radius;
}
}

bool Game::take_website_request() {
    const bool requested = website_request_;
    website_request_ = false;
    return requested;
}

void Game::return_to_title() {
    screen_ = Screen::title;
    menu_selection_ = 0;
    players_ = {};
    projectiles_.clear();
    enemies_.clear();
}

void Game::begin_game() {
    screen_ = Screen::playing;
    spawn_timer_ = 0;
    spawn_index_ = 0;
    for (std::size_t i = 0; i < players_.size(); ++i) {
        players_[i].x = 0.2f + static_cast<float>(i) * 0.2f;
        players_[i].y = 0.85f;
    }
}

void Game::update(float elapsed, const std::array<Input, max_devices>& input) {
    const float dt = std::clamp(elapsed, 0.0f, 0.05f);
    Input combined{};
    for (const auto& in : input) {
        combined.up |= in.up; combined.down |= in.down;
        combined.left |= in.left; combined.right |= in.right;
        combined.fire_pressed |= in.fire_pressed; combined.menu |= in.menu;
        combined.start |= in.start; combined.join |= in.join;
    }

    if (screen_ == Screen::title) {
        if (combined.up) menu_selection_ = (menu_selection_ + 3) % 4;
        if (combined.down) menu_selection_ = (menu_selection_ + 1) % 4;
        if (combined.fire_pressed || combined.start) {
            switch (menu_selection_) {
            case 0: screen_ = Screen::select; break;
            case 1: screen_ = Screen::credits; break;
            case 2: website_request_ = true; break;
            case 3: screen_ = Screen::options; menu_selection_ = 0; break;
            }
        }
        return;
    }
    if (screen_ == Screen::credits) {
        if (combined.fire_pressed || combined.menu || combined.start) return_to_title();
        return;
    }
    if (screen_ == Screen::options) {
        if (combined.menu) { return_to_title(); return; }
        if (combined.up || combined.down) menu_selection_ = 1 - menu_selection_;
        float& volume = menu_selection_ == 0 ? effects_volume_ : music_volume_;
        if (combined.left) volume = std::max(0.0f, volume - 0.1f);
        if (combined.right) volume = std::min(1.0f, volume + 0.1f);
        return;
    }
    if (screen_ == Screen::select) {
        if (combined.menu) { return_to_title(); return; }
        const auto all_joined_players_ready = [this] {
            return std::any_of(players_.begin(), players_.end(), [](const Player& p) { return p.joined; }) &&
                std::all_of(players_.begin(), players_.end(), [](const Player& p) { return !p.joined || p.ready; });
        };
        if (combined.start && all_joined_players_ready()) { begin_game(); return; }
        for (std::size_t device = 0; device < input.size(); ++device) {
            const auto& in = input[device];
            auto player = std::find_if(players_.begin(), players_.end(), [device](const Player& p) {
                return p.joined && p.device == static_cast<int>(device);
            });
            if (player == players_.end()) {
                if (!in.join) continue;
                player = std::find_if(players_.begin(), players_.end(), [](const Player& p) { return !p.joined; });
                if (player != players_.end()) {
                    *player = Player{};
                    player->joined = true;
                    player->device = static_cast<int>(device);
                }
                continue;
            }
            if (in.bomb) {
                if (player->ready) player->ready = false;
                else *player = Player{};
            } else if (!player->ready) {
                if (in.left) player->model = (player->model + 2) % 3;
                if (in.right) player->model = (player->model + 1) % 3;
                if (in.fire_pressed) player->ready = true;
            }
        }
        if (combined.start && all_joined_players_ready()) begin_game();
        return;
    }

    if (combined.menu) { return_to_title(); return; }
    for (auto& player : players_) {
        if (!player.joined) continue;
        const auto& in = input[static_cast<std::size_t>(player.device)];
        player.x = std::clamp(player.x + in.x * dt * 0.5f, 0.03f, 0.97f);
        player.y = std::clamp(player.y + in.y * dt * 0.5f, 0.03f, 0.97f);
        player.fire_cooldown = std::max(0.0f, player.fire_cooldown - dt);
        if (in.fire && player.fire_cooldown <= 0) {
            projectiles_.push_back({player.x, player.y - 0.03f, false});
            player.fire_cooldown = 0.15f;
        }
        if (in.bomb && player.bombs > 0) {
            projectiles_.push_back({player.x, player.y - 0.03f, true});
            --player.bombs;
        }
    }
    spawn_timer_ -= dt;
    if (spawn_timer_ <= 0) {
        enemies_.push_back({0.15f + 0.14f * static_cast<float>(spawn_index_++ % 6), -0.05f, 3});
        spawn_timer_ = 1.2f;
    }
    for (auto& enemy : enemies_) enemy.y += dt * 0.16f;
    for (auto& projectile : projectiles_) projectile.y -= dt * (projectile.bomb ? 0.35f : 0.8f);
    for (auto& projectile : projectiles_) {
        for (auto& enemy : enemies_) {
            if (enemy.health <= 0 || !close(projectile.x, projectile.y, enemy.x, enemy.y, 0.04f)) continue;
            if (projectile.bomb) {
                for (auto& target : enemies_)
                    if (close(projectile.x, projectile.y, target.x, target.y, 0.18f)) target.health -= 5;
            } else {
                --enemy.health;
            }
            projectile.y = -1;
            break;
        }
    }
    for (auto& enemy : enemies_) {
        if (enemy.health <= 0) continue;
        for (auto& player : players_) {
            if (!player.joined || !close(player.x, player.y, enemy.x, enemy.y, 0.04f)) continue;
            enemy.health = 0;
            if (--player.health <= 0) {
                --player.lives;
                player.health = 10;
                if (player.lives <= 0) { return_to_title(); return; }
            }
            break;
        }
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return p.y < -0.1f; });
    std::erase_if(enemies_, [](const Enemy& e) { return e.health <= 0 || e.y > 1.1f; });
}

}
