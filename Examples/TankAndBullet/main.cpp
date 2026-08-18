// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// TankAndBullet - a top-down tank arena reconstructed on the modern Game2D
// API, using the example's original assets (tankbody.png hull, tankhead.png
// turret, player.png enemies). WASD/arrows move, the mouse aims the turret,
// click or SPACE fires. Enemies drift in from the edges and chase you; every
// hit and kill kicks off GameJuice feedback (muzzle flash, hit sparks,
// shake, hit-stop, procedural SFX, floating scores). Run out of lives and
// the arena resets.
//
// Controls: WASD / arrows move | mouse aim | click / SPACE fire | R restart.
//
// The texture loads use graceful fallbacks so the binary runs from the repo
// root or from inside Examples/TankAndBullet/; a missing texture simply
// falls back to the drawn hull/turret primitives instead of crashing.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

SDL_Texture* loadTexture (SDL_Renderer* sdl, const std::string& name)
{
    const std::string paths[] = { "Examples/TankAndBullet/" + name, name };
    for (const auto& path : paths)
    {
        SDL_Surface* surface = IMG_Load (path.c_str());
        if (surface)
        {
            SDL_Texture* tex = SDL_CreateTextureFromSurface (sdl, surface);
            SDL_FreeSurface (surface);
            return tex;
        }
    }
    return nullptr;
}

} // namespace

class TankAndBullet : public Game2D
{
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WORLD_W = 700;
    static constexpr int WORLD_H = 700;
    static constexpr float PLAYER_SPEED = 260.0f;
    static constexpr float BULLET_SPEED = 520.0f;
    static constexpr float FIRE_COOLDOWN = 0.22f;
    static constexpr float BULLET_LIFE = 1.4f;
    static constexpr int MAX_BULLETS = 24;
    static constexpr float SPAWN_INTERVAL = 1.6f;
    static constexpr int MAX_ENEMIES = 6;

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;

    // ---- Entities -----------------------------------------------------------
    struct Bullet
    {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float life = 0.0f;
        bool active = false;
    };
    struct Enemy
    {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float turnTimer = 0.0f;
        int hp = 2;
        bool active = true;
    };

    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;

    // ---- Player state --------------------------------------------------------
    float playerX = WORLD_W * 0.5f;
    float playerY = WORLD_H * 0.5f;
    float aimX = WORLD_W * 0.5f;
    float aimY = 0.0f;
    int score = 0;
    int lives = 3;
    float fireCooldown = 0.0f;
    float spawnTimer = 0.0f;
    float invulnTimer = 0.0f;
    bool paused = false;

    SDL_Texture* hullTex = nullptr;
    SDL_Texture* turretTex = nullptr;
    SDL_Texture* enemyTex = nullptr;

    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

public:
    TankAndBullet () : Game2D ("Tank And Bullet", WORLD_W, WORLD_H, 20) {}

    void initGame() override
    {
        playerX = WORLD_W * 0.5f;
        playerY = WORLD_H * 0.5f;
        score = 0;
        lives = 3;
        fireCooldown = 0.0f;
        spawnTimer = 0.0f;
        invulnTimer = 0.0f;
        paused = false;
        bullets.clear();
        enemies.clear();
        particles.clear();
        floatTexts = uj::FloatingText{};

        SDL_Renderer* sdl = getRenderer()->renderer;
        hullTex = loadTexture (sdl, "tankbody.png");
        turretTex = loadTexture (sdl, "tankhead.png");
        enemyTex = loadTexture (sdl, "player.png");

        hud = createText (10, 6, "");
        hud->setColor ({255, 255, 255, 255});
        message = createText (10, WORLD_H - 26, "");
        message->setColor ({255, 220, 120, 255});

        bindKey (KEY_R).onPress ([this]()
        {
            if (!gameRunning || gameOver || gameWon) startGame();
        });
        bindKey (KEY_P).onPress ([this]() { paused = !paused; });

        registerAction ("up", [this]()
        {
            movePlayer (0.0f, -1.0f);
            return ActionResult{ true, "Moved up" };
        });
        registerAction ("down", [this]()
        {
            movePlayer (0.0f, 1.0f);
            return ActionResult{ true, "Moved down" };
        });
        registerAction ("left", [this]()
        {
            movePlayer (-1.0f, 0.0f);
            return ActionResult{ true, "Moved left" };
        });
        registerAction ("right", [this]()
        {
            movePlayer (1.0f, 0.0f);
            return ActionResult{ true, "Moved right" };
        });
        registerAction ("fire", [this]()
        {
            const bool fired = tryFire();
            return ActionResult{ fired, fired ? "Fired" : "Too soon / no bullets" };
        });
        registerAction ("restart", [this]()
        {
            startGame();
            return ActionResult{ true, "Restarted" };
        });

        updateHUD();
        setMessage ("WASD move | mouse aim | click/SPACE fire | R restart");
    }

    void updateGame (float dt) override
    {
        if (!gameRunning) return;
        if (paused)
        {
            if (message) message->setText ("PAUSED - press P to resume");
            return;
        }
        if (hitStop.frozen())
        {
            hitStop.update (dt);
            return;
        }

        // --- Input (continuous): keyboard state + mouse aim ----------------
        const Uint8* keys = SDL_GetKeyboardState (nullptr);
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState (&mouseX, &mouseY);
        aimX = static_cast<float>(mouseX);
        aimY = static_cast<float>(mouseY);

        float dx = 0.0f, dy = 0.0f;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) dy -= 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) dy += 1.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;
        movePlayer (dx, dy);

        fireCooldown -= dt;
        const bool wantFire = (SDL_GetMouseState (nullptr, nullptr) & SDL_BUTTON (1))
                           || keys[SDL_SCANCODE_SPACE];
        if (wantFire) (void) tryFire();

        // --- Enemies ---------------------------------------------------------
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f && static_cast<int>(enemies.size()) < MAX_ENEMIES)
        {
            spawnEnemy();
            spawnTimer = SPAWN_INTERVAL;
        }
        for (auto& e : enemies)
        {
            if (!e.active) continue;
            // Drift: pick a new direction every so often, biased toward the
            // player so the arena slowly closes in.
            e.turnTimer -= dt;
            if (e.turnTimer <= 0.0f)
            {
                const float tx = playerX - e.x, ty = playerY - e.y;
                const float len = std::sqrt (tx * tx + ty * ty);
                const float bx = len > 0.0f ? tx / len : 0.0f;
                const float by = len > 0.0f ? ty / len : 0.0f;
                const float rnd = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) - 0.5f;
                e.vx = bx * 60.0f + rnd * 90.0f;
                e.vy = by * 60.0f + rnd * 90.0f;
                e.turnTimer = 1.1f + (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 1.2f;
            }
            e.x += e.vx * dt;
            e.y += e.vy * dt;
            e.x = std::clamp (e.x, 0.0f, static_cast<float>(WORLD_W));
            e.y = std::clamp (e.y, 0.0f, static_cast<float>(WORLD_H));

            // Contact damage (with brief invulnerability so it is not instant).
            if (invulnTimer <= 0.0f)
            {
                const float ex = e.x - playerX, ey = e.y - playerY;
                if (ex * ex + ey * ey < 36.0f * 36.0f)
                {
                    hitPlayer();
                }
            }
        }

        // --- Bullets -----------------------------------------------------------
        for (auto& b : bullets)
        {
            if (!b.active) continue;
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            b.life -= dt;
            if (b.life <= 0.0f)
            {
                b.active = false;
                continue;
            }
            if (b.x < -20.0f || b.x > WORLD_W + 20.0f ||
                b.y < -20.0f || b.y > WORLD_H + 20.0f)
            {
                b.active = false;
                continue;
            }
            for (auto& e : enemies)
            {
                if (!e.active) continue;
                const float ex = e.x - b.x, ey = e.y - b.y;
                if (ex * ex + ey * ey < 22.0f * 22.0f)
                {
                    b.active = false;
                    e.hp -= 1;
                    // ---- Juice: hit spark + score pop ----------------------
                    particles.burst (b.x, b.y, 6, {255, 200, 80, 255}, 6.0f, 0.3f, 4.0f);
                    sfx.play (uj::Sfx::Hit);
                    if (e.hp <= 0)
                    {
                        e.active = false;
                        score += 100;
                        shake.add (0.35f);
                        hitStop.trigger (0.06f);
                        particles.burst (e.x, e.y, 16, {255, 90, 60, 255}, 9.0f, 0.6f, 5.0f);
                        sfx.play (uj::Sfx::Coin);
                        floatTexts.spawn (std::make_shared<TextDisplay> (
                            static_cast<int>(e.x) - 14, static_cast<int>(e.y) - 18, "+100"),
                            static_cast<int>(e.x) - 14, static_cast<int>(e.y) - 18);
                    }
                    break;
                }
            }
        }

        if (invulnTimer > 0.0f) invulnTimer -= dt;
        updateFx (dt);
        updateHUD();
    }

    void renderGame() override
    {
        SDL_Renderer* sdl = getRenderer()->renderer;
        const auto [sx, sy] = shake.offset();

        // Arena floor.
        SDL_Rect floor = { sx, sy, WORLD_W, WORLD_H };
        SDL_SetRenderDrawColor (sdl, 34, 40, 34, 255);
        SDL_RenderFillRect (sdl, &floor);
        SDL_SetRenderDrawColor (sdl, 70, 82, 62, 255);
        SDL_RenderDrawRect (sdl, &floor);

        // Enemies (player.png tinted red; flashing while weak).
        for (const auto& e : enemies)
        {
            if (!e.active) continue;
            drawTank (sdl, e.x, e.y, 46, enemyTex, e.hp == 1 ? 1.0f : 0.0f, sx, sy);
        }

        // Bullets (glowing yellow tracer).
        for (const auto& b : bullets)
        {
            if (!b.active) continue;
            SDL_Rect r = { static_cast<int>(b.x) - 4 + sx, static_cast<int>(b.y) - 4 + sy, 8, 8 };
            SDL_SetRenderDrawColor (sdl, 255, 230, 120, 255);
            SDL_RenderFillRect (sdl, &r);
        }

        // Player tank: hull, then the turret aimed at the mouse. Blinks
        // while invulnerable.
        if (invulnTimer > 0.0f && static_cast<int>(invulnTimer * 12.0f) % 2 == 0)
        {
            // blink frame: draw nothing
        }
        else
        {
            drawTank (sdl, playerX, playerY, 56, hullTex, 0.0f, sx, sy);
            const float angleDeg = std::atan2 (aimY - playerY, aimX - playerX) * 180.0f / static_cast<float>(M_PI);
            const SDL_Rect turret = {
                static_cast<int>(playerX) - 14 + sx, static_cast<int>(playerY) - 14 + sy, 28, 28 };
            SDL_Renderer* r = sdl;
            if (turretTex)
            {
                SDL_RenderCopyEx (r, turretTex, nullptr, &turret, angleDeg, nullptr, SDL_FLIP_NONE);
            }
            else
            {
                SDL_Rect barrel = { static_cast<int>(playerX) + sx, static_cast<int>(playerY) + sy, 26, 8 };
                SDL_SetRenderDrawColor (r, 200, 200, 210, 255);
                SDL_RenderFillRect (r, &barrel);
            }
        }

        particles.render (sdl, sx, sy);
        floatTexts.render (getRenderer());
        for (auto& t : textDisplays) t->render (getRenderer());
    }

    // ---- Helpers --------------------------------------------------------------
private:
    void movePlayer (float dx, float dy)
    {
        const float len = std::sqrt (dx * dx + dy * dy);
        if (len <= 0.0f) return;
        // Floor the step at one frame so LLM actions (which run outside the
        // render loop, where deltaTime is 0) still move the tank.
        const float dt = std::max (getDeltaTime(), 1.0f / 60.0f);
        playerX += dx / len * PLAYER_SPEED * dt;
        playerY += dy / len * PLAYER_SPEED * dt;
        playerX = std::clamp (playerX, 24.0f, static_cast<float>(WORLD_W - 24));
        playerY = std::clamp (playerY, 24.0f, static_cast<float>(WORLD_H - 24));
    }

    bool tryFire()
    {
        if (fireCooldown > 0.0f) return false;
        Bullet* slot = nullptr;
        for (auto& b : bullets)
        {
            if (!b.active) { slot = &b; break; }
        }
        if (!slot && static_cast<int>(bullets.size()) < MAX_BULLETS)
        {
            bullets.push_back (Bullet{});
            slot = &bullets.back();
        }
        if (!slot) return false;

        float tx = aimX - playerX, ty = aimY - playerY;
        const float len = std::sqrt (tx * tx + ty * ty);
        if (len <= 0.0f) { tx = 1.0f; ty = 0.0f; }
        else { tx /= len; ty /= len; }

        slot->x = playerX + tx * 34.0f;
        slot->y = playerY + ty * 34.0f;
        slot->vx = tx * BULLET_SPEED;
        slot->vy = ty * BULLET_SPEED;
        slot->life = BULLET_LIFE;
        slot->active = true;
        fireCooldown = FIRE_COOLDOWN;

        // ---- Juice: muzzle flash + shot -----------------------------------
        particles.burst (slot->x, slot->y, 5, {255, 220, 140, 255}, 5.0f, 0.25f, 4.0f);
        sfx.play (uj::Sfx::Shoot);
        return true;
    }

    void spawnEnemy()
    {
        Enemy e;
        const int side = std::rand() % 4;
        if (side == 0)      { e.x = -30.0f; e.y = static_cast<float>(std::rand() % WORLD_H); }
        else if (side == 1) { e.x = WORLD_W + 30.0f; e.y = static_cast<float>(std::rand() % WORLD_H); }
        else if (side == 2) { e.x = static_cast<float>(std::rand() % WORLD_W); e.y = -30.0f; }
        else                { e.x = static_cast<float>(std::rand() % WORLD_W); e.y = WORLD_H + 30.0f; }
        e.hp = 2;
        enemies.push_back (e);
    }

    void hitPlayer()
    {
        lives -= 1;
        invulnTimer = 1.2f;
        shake.add (0.6f);
        hitStop.trigger (0.12f);
        particles.burst (playerX, playerY, 20, {255, 120, 60, 255}, 10.0f, 0.7f, 6.0f);
        sfx.play (uj::Sfx::Lose);
        if (lives <= 0)
        {
            setMessage ("GAME OVER - Score " + std::to_string (score) + " - Press R to restart");
            endGame();
        }
        else
        {
            setMessage ("HIT! " + std::to_string (lives) + " lives left");
        }
    }

    void drawTank (SDL_Renderer* sdl, float x, float y, int size,
                   SDL_Texture* tex, float tint, int sx, int sy) const
    {
        const SDL_Rect r = {
            static_cast<int>(x) - size / 2 + sx, static_cast<int>(y) - size / 2 + sy, size, size };
        if (tex)
        {
            if (tint > 0.0f)
            {
                const Uint8 c = static_cast<Uint8> (255.0f * (1.0f - tint * 0.6f));
                SDL_SetTextureColorMod (tex, 255, c, c);
            }
            SDL_RenderCopy (sdl, tex, nullptr, &r);
        }
        else
        {
            SDL_SetRenderDrawColor (sdl, 120, 130, 110, 255);
            SDL_RenderFillRect (sdl, &r);
            SDL_SetRenderDrawColor (sdl, 60, 66, 54, 255);
            SDL_RenderDrawRect (sdl, &r);
        }
    }

    void updateFx (float dt)
    {
        particles.update (dt);
        floatTexts.update (dt);
        shake.update (dt);
    }

    void setMessage (const std::string& text)
    {
        if (message) message->setText (text);
    }

    void updateHUD()
    {
        if (!hud) return;
        hud->setText ("Score " + std::to_string (score) +
                      "    Lives " + std::to_string (lives) +
                      "    Enemies " + std::to_string (enemies.size()));
    }
};

int main ([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static TankAndBullet game;
#else
    TankAndBullet game;
#endif
    game.run();
    return 0;
}
