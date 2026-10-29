#include "core/Game.h"
#include "core/Util.h"
#include "gfx/IconData.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace {
    const float       FX_VALUES[4] = { 0.f, 0.35f, 0.65f, 1.f };
    const char* const FX_NAMES[4]  = { "APAGADOS", "SUAVES", "MEDIOS", "INTENSOS" };
}

Game::Game() {}
Game::~Game() {}

void Game::Init() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Cfg::WINDOW_W, Cfg::WINDOW_H, "CYBER-NEMESIS : Adaptive Boss Protocol");
    {
        Image icon{};
        icon.data = const_cast<unsigned char*>(Embedded::ICON_RGBA);
        icon.width = 32; icon.height = 32; icon.mipmaps = 1;
        icon.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        SetWindowIcon(icon);
    }
    SetWindowMinSize(Cfg::VIEW_W, Cfg::VIEW_H);
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);

    sprites_.Init();
    fx_.Init();
    audio_.Init();
    fx_.SetIntensity(FX_VALUES[fxLevel_]);
    bg_.Init(1337u);
    shards_.Init(2024u);
    SetRandomSeed((unsigned)std::time(nullptr));

    brainLoaded_ = brain_.Load();
    hero_.ApplyTraits(brain_.Traits());
    boss_.SetScale(Cfg::BossScaleForGeneration(brain_.Traits().generation), true);
    bg_.SetTheme(brain_.Traits().generation);
    ResetRound();
    state_ = State::Title;
}

void Game::Shutdown() {
    brain_.Save();
    audio_.Shutdown();
    fx_.Shutdown();
    sprites_.Shutdown();
    CloseWindow();
}

void Game::Run() {
    Init();
    while (!WindowShouldClose() && !quit_) {
        const float dt = std::min(GetFrameTime(), 1.f / 30.f);
        Update(dt);
        Draw();
    }
    Shutdown();
}

Rectangle Game::PresentRect() const {
    const float w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
    const float s = std::min(w / Cfg::VIEW_W, h / Cfg::VIEW_H);
    return { (w - Cfg::VIEW_W * s) * 0.5f, (h - Cfg::VIEW_H * s) * 0.5f, Cfg::VIEW_W * s, Cfg::VIEW_H * s };
}

Vector2 Game::MouseToView() const {
    const Rectangle r = PresentRect();
    const float s = r.width / Cfg::VIEW_W;
    const Vector2 m = GetMousePosition();
    return { (m.x - r.x) / s, (m.y - r.y) / s };
}

void Game::SpawnExplosion(Vector2 p, Color a, Color b, int n) {
    particles_.Burst(p, n, a, 260.f, 4.f, 0.9f, true);
    particles_.Burst(p, n / 2, b, 160.f, 3.f, 1.2f, false);
    particles_.Ring(p, 24, WHITE, 220.f, 2.f, 0.5f);
}

BossInput Game::IdleBossInput(float lookAtX) const {
    BossInput in;
    in.aim = { lookAtX, boss_.pos.y - 50.f };
    return in;
}

void Game::Update(float dt) {
    time_ += dt;
    if (IsKeyPressed(KEY_M)) audio_.ToggleMusic();
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
    if (IsKeyPressed(KEY_F10)) {
        fxLevel_ = (fxLevel_ + 1) % 4;
        fx_.SetIntensity(FX_VALUES[fxLevel_]);
        fxMsgT_ = 2.f;
    }
    fxMsgT_ = std::max(0.f, fxMsgT_ - dt);
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (state_ == State::Title) quit_ = true;
        else ReturnToMenu();
        return;
    }
    cevLabelT_ = std::max(0.f, cevLabelT_ - dt);
    phaseBannerT_ = std::max(0.f, phaseBannerT_ - dt);
    tierBannerT_  = std::max(0.f, tierBannerT_ - dt);
    Entity::worldGravity = Lerp(Entity::worldGravity, gravTarget_, 1.f - std::exp(-dt * 3.f));
    if (IsKeyPressed(KEY_P) && state_ == State::Playing) paused_ = !paused_;

    glitch_ = std::max(0.f, glitch_ - dt * 2.5f);
    shake_  = std::max(0.f, shake_ - dt * 28.f);
    shakeOff_ = { Util::RandRange(-1.f, 1.f) * shake_, Util::RandRange(-1.f, 1.f) * shake_ };
    hintTimer_ += dt;

    const float gen = (float)brain_.Traits().generation;
    audio_.UpdateMusic(final_ ? 1.6f : Util::Clamp01((gen - 1.f) / (float)(Cfg::MAX_GENERATION - 1)));

    if (paused_) return;
    bg_.Update(dt);
    if (bg_.ConsumeLightning()) {
        audio_.Play(AudioManager::Thunder, 0.8f);
        glitch_ = std::max(glitch_, 0.5f);
    }

    switch (state_) {
        case State::Title:     UpdateTitle(dt);     break;
        case State::Playing:   UpdatePlaying(dt);   break;
        case State::HeroDying: UpdateHeroDying(dt); break;
        case State::Analysis:  UpdateAnalysis(dt);  break;
        case State::FinalIntro: UpdateFinalIntro(dt); break;
        case State::BossDown:  UpdateBossDown(dt);  break;
        case State::Victory:   UpdateVictory(dt);   break;
    }
}

void Game::UpdateTitle(float dt) {
    stateTime_ += dt;
    const Vector2 m = MouseToView();
    shards_.SetTint(Util::LerpColor(Color{150, 105, 225, 255}, bg_.GridColor(), 0.35f));
    shards_.Update(dt, { Clamp((m.x / Cfg::VIEW_W - 0.5f) * 2.f, -1.f, 1.f),
                         Clamp((m.y / Cfg::VIEW_H - 0.5f) * 2.f, -1.f, 1.f) });

    const MainMenu::Action act = menu_.Update(dt, m, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    if (menu_.ConsumeMoved()) audio_.Play(AudioManager::Blip, 0.35f, 1.4f);

    switch (act) {
        case MainMenu::Action::Start:
            audio_.Play(AudioManager::Blip, 0.6f, 1.8f);
            StartRound(false);
            break;
        case MainMenu::Action::Effects:
            fxLevel_ = (fxLevel_ + 1) % 4;
            fx_.SetIntensity(FX_VALUES[fxLevel_]);
            audio_.Play(AudioManager::Blip, 0.5f, 1.2f);
            break;
        case MainMenu::Action::Music:
            audio_.ToggleMusic();
            audio_.Play(AudioManager::Blip, 0.5f, 1.2f);
            break;
        case MainMenu::Action::Fullscreen:
            ToggleFullscreen();
            break;
        case MainMenu::Action::Wipe:
            brain_.Reset();
            hero_.ApplyTraits(brain_.Traits());
            boss_.SetScale(1.f, true);
            bg_.SetTheme(1);
            brainLoaded_ = false;
            audio_.Play(AudioManager::Explosion, 0.4f, 1.5f);
            glitch_ = 0.8f;
            break;
        case MainMenu::Action::Quit:
            quit_ = true;
            break;
        default: break;
    }
}

void Game::ReturnToMenu() {
    brain_.Save();
    hero_.ApplyTraits(brain_.Traits());
    boss_.SetScale(Cfg::BossScaleForGeneration(brain_.Traits().generation), true);
    bg_.SetTheme(brain_.Traits().generation);
    final_ = false;
    bg_.SetEventsEnabled(true);
    ResetRound();
    state_ = State::Title;
    stateTime_ = 0.f;
    paused_ = false;
    introTimer_ = 0.f;
}

BossInput Game::ReadBossInput() const {
    BossInput in;
    in.moveDir = (float)((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)));
    in.jumpPressed = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE);
    in.jumpHeld    = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE);
    in.dash        = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_L);
    in.down        = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    in.melee       = IsKeyDown(KEY_J) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in.shoot       = IsKeyDown(KEY_K) || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.aim         = MouseToView();
    return in;
}

void Game::ResetRound() {
    arena_.Reset();
    boss_.Reset(150.f);
    hero_.Reset(500.f);
    projectiles_.clear();
    shockwaves_.clear();
    hazards_.Clear();
    bullets_.Clear();
    drones_.Clear();
    pickups_.Clear();
    particles_.Clear();
    phase_ = 0;
    cev_ = lastCev_ = CEvent::None;
    eventTimer_ = Cfg::EVENT_FIRST_DELAY + Util::RandRange(0.f, 6.f);
    gravTarget_ = 1.f;
    Entity::worldGravity = 1.f;
    cevLabelT_ = phaseBannerT_ = 0.f;
    shrinkAnnounced_ = false;
    hero_.SetThreatColumns({});
    tracker_.Reset();
    hud_.Reset(boss_, hero_);
    nextId_ = 1;
    hitStop_ = 0.f;
    slowmo_ = 1.f;
    shake_ = 0.f;
}

void Game::StartRound(bool newGeneration) {
    final_ = false;
    bg_.SetEventsEnabled(true);
    hero_.ApplyTraits(brain_.Traits());
    const float oldScale = boss_.Scale();
    const float newScale = Cfg::BossScaleForGeneration(brain_.Traits().generation);
    ResetRound();
    introShrink_ = newGeneration && newScale < oldScale - 0.001f;
    if (introShrink_) boss_.BeginShrink(oldScale, newScale);
    else boss_.SetScale(newScale, true);
    bg_.SetTheme(brain_.Traits().generation);
    state_ = State::Playing;
    stateTime_ = 0.f;
    introTimer_ = 2.2f;
    hintTimer_ = 0.f;
    audio_.Play(AudioManager::HeroSpawn);
    particles_.Ring({ hero_.pos.x, hero_.pos.y - 32.f }, 30, Theme::NEON_CYAN, 200.f, 3.f, 0.7f);
    if (newGeneration) glitch_ = 1.f;
}

void Game::UpdatePlaying(float dt) {
    stateTime_ += dt;
    hitStop_ = std::max(0.f, hitStop_ - dt);

    if (introTimer_ > 0.f) {
        introTimer_ -= dt;
        if (introTimer_ < 1.7f) {
            if (boss_.IsShrinking() && !shrinkAnnounced_) {
                shrinkAnnounced_ = true;
                audio_.Play(AudioManager::Dash, 0.5f, 0.6f);
                particles_.Ring(boss_.Center(), 20, Theme::NEON_MAGENTA, 160.f, 2.f, 0.5f);
            }
            if (boss_.IsShrinking()) particles_.Burst(boss_.Center(), 2, Theme::NEON_MAGENTA, 80.f, 2.f, 0.4f);
            boss_.UpdateScale(dt);
        }
        arena_.Update(dt);
        particles_.Update(dt);
        hud_.Update(dt, boss_, hero_);
        return;
    }

    const float sdt = dt * (hitStop_ > 0.f ? 0.05f : 1.f);
    arena_.Update(sdt);
    boss_.Update(sdt, ReadBossInput(), arena_);
    hero_.SetThreatColumns(hazards_.NeutralColumns());
    hero_.Update(sdt, boss_, projectiles_, arena_);
    tracker_.Sample(boss_, sdt);

    HandleBossEvents();
    HandleHeroEvents();
    if (final_) UpdateFinalPhase(sdt);
    else { UpdateCombatEvents(sdt); UpdateDrones(sdt); }
    if (pickups_.Update(sdt, arena_, boss_.HurtBox())) {
        boss_.Heal(Cfg::CORE_HEAL);
        boss_.Overcharge(Cfg::OVERCHARGE_TIME);
        particles_.Ring(boss_.Center(), 28, Theme::NEON_YELLOW, 220.f, 3.f, 0.7f);
        particles_.Burst(boss_.Center(), 20, WHITE, 200.f, 2.f, 0.5f);
        audio_.Play(AudioManager::ShieldHit, 0.8f, 1.6f);
        cevLabel_ = "SOBRECARGA ACTIVADA: +40% DANO Y RECARGAS RAPIDAS"; cevLabelT_ = 3.f;
        glitch_ = std::max(glitch_, 0.3f);
    }
    UpdateProjectiles(sdt);
    UpdateHazards(sdt);
    particles_.Update(sdt);
    hud_.Update(dt, boss_, hero_);

    for (auto& s : shockwaves_) s.t += sdt;
    shockwaves_.erase(std::remove_if(shockwaves_.begin(), shockwaves_.end(),
                      [](const Shockwave& s) { return s.t > 0.4f; }), shockwaves_.end());

    if (!hero_.Alive()) OnHeroDefeated();
    else if (!boss_.Alive()) OnBossDefeated();
    else {
        const float frac = hero_.hp / hero_.maxHp;
        if (final_) {   }
        else if (phase_ == 0 && frac < Cfg::PHASE1_FRAC) TriggerPhase(1);
        else if (phase_ == 1 && frac < Cfg::PHASE2_FRAC) TriggerPhase(2);
    }
}

void Game::HandleBossEvents() {
    Vector2 o, d;
    if (boss_.ConsumeShot(o, d)) {
        Projectile p;
        p.pos = o; p.vel = Vector2Scale(d, Cfg::BOSS_SHOT_SPEED);
        p.radius = 10.f; p.damage = Cfg::BOSS_SHOT_DAMAGE * boss_.DamageMul(); p.owner = Owner::Boss; p.kind = ProjKind::Plasma;
        p.id = nextId_++;
        projectiles_.push_back(p);
        tracker_.RecordRanged();
        audio_.Play(AudioManager::BossShot, 0.8f);
        particles_.Directional(o, d, 0.5f, 8, Theme::NEON_MAGENTA, 180.f, 3.f, 0.3f);
        shake_ = std::max(shake_, 1.5f);
    }

    if (const int j = boss_.ConsumeJump()) {
        tracker_.RecordJump();
        const Vector2 feet = { boss_.pos.x, boss_.pos.y };
        if (j == 2) {
            particles_.Ring({ feet.x, feet.y - 4.f }, 16, Theme::NEON_CYAN, 140.f, 2.f, 0.4f);
            audio_.Play(AudioManager::Jump, 0.5f, 1.4f);
        } else {
            particles_.Directional(feet, { 0.f, -1.f }, 1.4f, 10, Theme::NEON_CYAN, 120.f, 3.f, 0.4f);
            audio_.Play(AudioManager::Jump, 0.4f, 0.8f);
        }
    }

    if (boss_.ConsumeDash()) {
        tracker_.RecordDash();
        audio_.Play(AudioManager::Dash, 0.6f, 0.8f);
        particles_.Burst(boss_.Center(), 16, Theme::NEON_MAGENTA, 220.f, 3.f, 0.4f);
        shake_ = std::max(shake_, 2.f);
    }

    if (boss_.ConsumeSlam()) {
        tracker_.RecordMelee();
        audio_.Play(AudioManager::Slam);
        shake_ = std::max(shake_, 8.f);
        const float gy = boss_.pos.y;
        const float edge = boss_.w * 0.5f + 8.f;
        shockwaves_.push_back({ { boss_.pos.x, gy }, 0.f });
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            particles_.Directional({ boss_.pos.x + sgn * edge, gy - 2.f }, { (float)sgn, -0.35f },
                                   0.5f, 16, Theme::NEON_MAGENTA, 330.f, 3.f, 0.5f, true);
            particles_.Directional({ boss_.pos.x + sgn * edge, gy - 2.f }, { (float)sgn, -0.15f },
                                   0.3f, 10, Theme::NEON_CYAN, 380.f, 2.f, 0.4f, false);
        }
        std::vector<HazardEvent> ev;
        hazards_.DestroyMinesIn(boss_.MeleeZone(), ev);
        for (const auto& e : ev) particles_.Burst(e.pos, 10, Theme::NEON_YELLOW, 160.f, 2.f, 0.4f);
        drones_.DestroyIn(boss_.MeleeZone());
        if (bullets_.ClearIn(boss_.MeleeZone()) > 0)
            particles_.Ring({ boss_.pos.x, boss_.pos.y - 10.f }, 20, Theme::NEON_CYAN, 200.f, 2.f, 0.4f);

        if (hero_.Alive() && CheckCollisionRecs(boss_.MeleeZone(), hero_.Bounds())) {
            float dealt = hero_.TakeDamage(Cfg::BOSS_MELEE_DAMAGE * boss_.DamageMul(), false);
            if (dealt >= 0.f) {
                SpawnExplosion(hero_.Center(), Theme::NEON_RED, Theme::NEON_YELLOW, 18);
                hitStop_ = 0.09f; glitch_ = 1.f; shake_ = 11.f;
                audio_.Play(AudioManager::Hit);
            }
        }
    }
}

void Game::HandleHeroEvents() {
    bool firstPellet = true, firstMissile = true;
    for (const ShotCmd& s : hero_.TakeShots()) {
        projectiles_.push_back(MakeHeroProjectile(s));
        if (s.kind == ProjKind::Missile) {
            if (firstMissile) audio_.Play(AudioManager::Missile, 0.6f);
            firstMissile = false;
        } else if (s.kind == ProjKind::Pellet) {
            if (firstPellet) audio_.Play(AudioManager::HeroShot, 0.6f, 0.6f);
            firstPellet = false;
            particles_.Directional(s.origin, s.dir, 0.3f, 2, Theme::NEON_YELLOW, 160.f, 2.f, 0.2f);
        } else {
            audio_.Play(AudioManager::HeroShot, 0.4f);
            particles_.Directional(s.origin, s.dir, 0.4f, 3, Theme::NEON_CYAN, 120.f, 2.f, 0.2f);
        }
    }

    float x, dmg; Vector2 mp;
    if (hero_.ConsumeOrbital(x, dmg)) {
        hazards_.SpawnOrbital(x, dmg);
        audio_.Play(AudioManager::Analyze, 0.5f, 1.2f);
    }
    if (hero_.ConsumeMine(mp, dmg)) {
        hazards_.SpawnMine(mp, dmg);
        audio_.Play(AudioManager::Blip, 0.7f, 0.7f);
        particles_.Burst({ mp.x, mp.y - 4.f }, 6, Theme::NEON_YELLOW, 80.f, 2.f, 0.3f);
    }
    if (hero_.ConsumeDash()) {
        audio_.Play(AudioManager::Dash, 0.7f);
        particles_.Burst(hero_.Center(), 14, Theme::NEON_CYAN, 180.f, 3.f, 0.4f);
    }
    if (const int j = hero_.ConsumeJump()) {
        audio_.Play(AudioManager::Jump, 0.4f, j == 2 ? 1.5f : 1.f);
        if (j == 2) particles_.Burst({ hero_.pos.x, hero_.pos.y }, 10, Theme::NEON_YELLOW, 140.f, 2.f, 0.35f);
        else particles_.Directional({ hero_.pos.x, hero_.pos.y }, { 0.f, -1.f }, 1.2f, 6, Theme::NEON_CYAN, 90.f, 2.f, 0.3f);
    }
}

void Game::UpdateProjectiles(float dt) {
    const Vector2 bossTarget = boss_.Center();
    for (auto& p : projectiles_) {
        if (!p.alive) continue;
        p.Update(dt, bossTarget);
        if (!p.alive) {
            if (p.pos.y >= Cfg::GROUND_Y - 1.f) {
                const Color c = p.owner == Owner::Boss ? Theme::NEON_MAGENTA : Theme::NEON_CYAN;
                if (p.kind == ProjKind::Missile) { SpawnExplosion({ p.pos.x, Cfg::GROUND_Y - 4.f }, Theme::NEON_YELLOW, Theme::NEON_RED, 16); audio_.Play(AudioManager::Explosion, 0.4f); }
                else particles_.Directional({ p.pos.x, Cfg::GROUND_Y }, { 0.f, -1.f }, 1.2f, 8, c, 150.f, 2.f, 0.3f, true);
            }
            continue;
        }

        if (p.owner == Owner::Boss) {
            if (drones_.HitCircle(p.pos, p.radius, p.damage)) {
                p.alive = false;
                particles_.Burst(p.pos, 6, Theme::NEON_MAGENTA, 140.f, 2.f, 0.3f);
                continue;
            }
            if (hero_.Alive() && CheckCollisionCircleRec(p.pos, p.radius, hero_.Bounds())) {
                const bool shielded = hero_.Traits().deflectShield;
                float dealt = hero_.TakeDamage(p.damage, true);
                if (dealt >= 0.f) {
                    p.alive = false;
                    SpawnExplosion(p.pos, shielded ? Theme::NEON_CYAN : Theme::NEON_MAGENTA, WHITE, shielded ? 10 : 16);
                    audio_.Play(shielded ? AudioManager::ShieldHit : AudioManager::Hit);
                    shake_ = std::max(shake_, 4.f);
                    hitStop_ = shielded ? 0.f : 0.05f;
                    glitch_ = std::max(glitch_, shielded ? 0.2f : 0.7f);
                }
            }
        } else {
            if (boss_.Alive() && CheckCollisionCircleRec(p.pos, p.radius, boss_.HurtBox())) {
                boss_.TakeDamage(p.damage);
                p.alive = false;
                if (p.kind == ProjKind::Missile) {
                    SpawnExplosion(p.pos, Theme::NEON_YELLOW, Theme::NEON_RED, 22);
                    audio_.Play(AudioManager::Explosion, 0.7f);
                    shake_ = std::max(shake_, 6.f); glitch_ = std::max(glitch_, 0.4f);
                } else {
                    particles_.Burst(p.pos, 8, Theme::NEON_CYAN, 150.f, 2.f, 0.3f);
                    particles_.Burst(p.pos, 4, WHITE, 100.f, 2.f, 0.2f);
                    audio_.Play(AudioManager::Hit, 0.35f, 1.4f);
                    shake_ = std::max(shake_, 1.5f);
                }
            }
        }
    }

    for (auto& bp : projectiles_) {
        if (!bp.alive || bp.owner != Owner::Boss) continue;
        for (auto& mp : projectiles_) {
            if (!mp.alive || mp.owner != Owner::Hero || mp.kind != ProjKind::Missile) continue;
            if (CheckCollisionCircles(bp.pos, bp.radius, mp.pos, mp.radius + 4.f)) {
                bp.alive = mp.alive = false;
                SpawnExplosion(mp.pos, Theme::NEON_YELLOW, Theme::NEON_MAGENTA, 18);
                audio_.Play(AudioManager::Explosion, 0.5f);
                break;
            }
        }
    }

    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                       [](const Projectile& p) { return !p.alive; }), projectiles_.end());
}

void Game::UpdateHazards(float dt) {
    std::vector<HazardEvent> events;
    hazards_.Update(dt, boss_.HurtBox(), boss_.pos.y, events);
    for (const auto& e : events) {
        switch (e.type) {
            case HazardEvent::MineBlast:
                SpawnExplosion(e.pos, Theme::NEON_RED, Theme::NEON_YELLOW, 26);
                audio_.Play(AudioManager::Explosion);
                shake_ = std::max(shake_, 8.f); glitch_ = std::max(glitch_, 0.6f);
                if (boss_.Alive() && CheckCollisionCircleRec(e.pos, e.radius, boss_.HurtBox())) {
                    boss_.TakeDamage(e.damage);
                    hitStop_ = std::max(hitStop_, 0.05f);
                }
                break;
            case HazardEvent::MineFizzle:
                particles_.Burst(e.pos, 8, Theme::NEON_YELLOW, 120.f, 2.f, 0.3f);
                break;
            case HazardEvent::OrbitalFire: {
                audio_.Play(AudioManager::Beam);
                shake_ = std::max(shake_, 9.f); glitch_ = std::max(glitch_, 0.8f);
                for (int i = 0; i < 6; ++i)
                    particles_.Burst({ e.pos.x, Cfg::GROUND_Y - 4.f }, 6, Theme::NEON_CYAN, 260.f, 3.f, 0.5f, true);
                const Rectangle col = { e.pos.x - e.radius, 0.f, e.radius * 2.f, Cfg::GROUND_Y };
                if (e.neutral && hero_.Alive() && CheckCollisionRecs(col, hero_.Bounds())) {
                    if (hero_.TakeDamage(e.damage, false) >= 0.f) {
                        SpawnExplosion(hero_.Center(), Theme::NEON_YELLOW, Theme::NEON_RED, 16);
                        audio_.Play(AudioManager::Hit);
                    }
                }
                if (boss_.Alive() && CheckCollisionRecs(col, boss_.HurtBox())) {
                    boss_.TakeDamage(e.damage);
                    SpawnExplosion(boss_.Center(), Theme::NEON_CYAN, WHITE, 22);
                    hitStop_ = std::max(hitStop_, 0.08f);
                }
                break;
            }
        }
    }
}

Projectile Game::MakeHeroProjectile(const ShotCmd& s) {
    Projectile p;
    p.pos = s.origin; p.owner = Owner::Hero; p.kind = s.kind; p.homing = s.homing;
    p.maxSpeed = s.speed;
    p.vel = Vector2Scale(s.dir, s.homing ? s.speed * 0.55f : s.speed);
    p.radius = s.radius; p.damage = s.damage; p.life = s.life; p.id = nextId_++;
    return p;
}

Game::CEvent Game::PickCombatEvent() const {
    float wB = 3.f, wD = 2.f, wG = 2.f, wC = pickups_.Active() ? 0.f : (boss_.hp < boss_.maxHp * 0.7f ? 4.f : 1.5f);
    if (drones_.Count() > 0) wD = 0.f;
    if (lastCev_ == CEvent::Bombard) wB = 0.f;
    if (lastCev_ == CEvent::Drones)  wD = 0.f;
    if (lastCev_ == CEvent::LowGrav) wG = 0.f;
    if (lastCev_ == CEvent::Core)    wC = 0.f;
    float roll = Util::Rand01() * (wB + wD + wG + wC);
    if ((roll -= wB) < 0.f && wB > 0.f) return CEvent::Bombard;
    if ((roll -= wD) < 0.f && wD > 0.f) return CEvent::Drones;
    if ((roll -= wG) < 0.f && wG > 0.f) return CEvent::LowGrav;
    return wC > 0.f ? CEvent::Core : CEvent::Bombard;
}

void Game::StartCombatEvent(CEvent e, bool fromPhase) {
    lastCev_ = e;
    cevT_ = 0.f;
    audio_.Play(AudioManager::Analyze, 0.5f, 0.8f);
    glitch_ = std::max(glitch_, 0.35f);
    cevLabelT_ = 3.2f;
    eventTimer_ = Util::RandRange(17.f, 27.f);
    switch (e) {
        case CEvent::Bombard:
            cev_ = CEvent::Bombard;
            strikesLeft_ = fromPhase ? 6 : 4;
            strikeTimer_ = 0.4f;
            cevDur_ = (float)strikesLeft_ * 0.75f + 1.5f;
            cevLabel_ = "EVENTO DE COMBATE: BOMBARDEO - HUYE DE LAS ZONAS NARANJAS";
            break;
        case CEvent::Drones:
            drones_.Spawn(fromPhase ? 3 : 2, hero_.Center(), hero_.Traits().damage * 0.8f);
            cevLabel_ = "EVENTO DE COMBATE: REFUERZOS - DRONES DEL ANDROIDE";
            break;
        case CEvent::LowGrav:
            cev_ = CEvent::LowGrav;
            cevDur_ = 9.f;
            gravTarget_ = 0.6f;
            cevLabel_ = "EVENTO DE COMBATE: FLUJO DE GRAVEDAD REDUCIDA";
            break;
        case CEvent::Core:
            pickups_.SpawnCore(arena_);
            cevLabel_ = "EVENTO DE COMBATE: NUCLEO DE ENERGIA - RECOGELO";
            break;
        default: break;
    }
}

void Game::EndCombatEvent() {
    cev_ = CEvent::None;
    gravTarget_ = 1.f;
    eventTimer_ = Util::RandRange(17.f, 27.f);
}

void Game::UpdateCombatEvents(float dt) {
    eventTimer_ -= dt;
    if (cev_ == CEvent::None && eventTimer_ <= 0.f) StartCombatEvent(PickCombatEvent(), false);

    if (cev_ == CEvent::Bombard) {
        cevT_ += dt;
        strikeTimer_ -= dt;
        if (strikesLeft_ > 0 && strikeTimer_ <= 0.f) {
            strikeTimer_ = 0.75f;
            float x = (strikesLeft_ % 2 == 0) ? boss_.pos.x + Util::RandRange(-40.f, 40.f) : Util::RandRange(40.f, 600.f);
            x = Clamp(x, 30.f, (float)Cfg::VIEW_W - 30.f);
            hazards_.SpawnStrike(x, 38.f);
            --strikesLeft_;
            audio_.Play(AudioManager::Blip, 0.6f, 0.5f);
        }
        if (strikesLeft_ <= 0 && cevT_ > cevDur_) EndCombatEvent();
    } else if (cev_ == CEvent::LowGrav) {
        cevT_ += dt;
        if (GetRandomValue(0, 5) == 0)
            particles_.Directional({ Util::RandRange(10.f, 630.f), Cfg::GROUND_Y - 2.f }, { 0.f, -1.f }, 0.2f, 1,
                                   Theme::NEON_CYAN, 50.f, 2.f, 1.4f);
        if (cevT_ > cevDur_) EndCombatEvent();
    }
}

void Game::UpdateDrones(float dt) {
    std::vector<ShotCmd> shots;
    drones_.Update(dt, boss_.Center(), shots);
    for (const ShotCmd& s : shots) {
        projectiles_.push_back(MakeHeroProjectile(s));
        audio_.Play(AudioManager::HeroShot, 0.25f, 1.7f);
    }
    drones_.DestroyIn(boss_.HurtBox());
    for (const Vector2& d : drones_.TakeDeaths()) {
        SpawnExplosion(d, Theme::NEON_RED, Theme::NEON_YELLOW, 10);
        audio_.Play(AudioManager::Explosion, 0.35f, 1.3f);
    }
}

void Game::TriggerPhase(int phase) {
    phase_ = phase;
    hero_.PhaseShift();
    boss_.Knock(boss_.pos.x < hero_.pos.x ? -520.f : 520.f, -240.f);
    particles_.Ring(hero_.Center(), 36, Theme::NEON_RED, 320.f, 3.f, 0.7f);
    particles_.Ring(hero_.Center(), 24, WHITE, 200.f, 2.f, 0.5f);
    audio_.Play(AudioManager::Beam, 0.8f);
    shake_ = std::max(shake_, 10.f);
    glitch_ = std::max(glitch_, 0.8f);
    hitStop_ = std::max(hitStop_, 0.1f);
    phaseBannerT_ = 2.4f;
    phaseBannerNum_ = phase + 1;
    StartCombatEvent(phase == 1 ? CEvent::Drones : CEvent::Bombard, true);
}

void Game::OnHeroDefeated() {
    bullets_.Clear();
    gravTarget_ = 1.f;
    cev_ = CEvent::None;
    state_ = State::HeroDying;
    stateTime_ = 0.f;
    slowmo_ = 0.3f;
    shake_ = 14.f;
    glitch_ = 1.f;
    SpawnExplosion(hero_.Center(), Theme::NEON_CYAN, Theme::NEON_MAGENTA, 70);
    audio_.Play(AudioManager::HeroDeath);
}

void Game::OnBossDefeated() {
    state_ = State::BossDown;
    stateTime_ = 0.f;
    slowmo_ = 0.25f;
    shake_ = 16.f;
    glitch_ = 1.f;
    SpawnExplosion(boss_.Center(), Theme::NEON_MAGENTA, Theme::NEON_YELLOW, 120);
    audio_.Play(AudioManager::BossDeath);
}

void Game::UpdateHeroDying(float dt) {
    stateTime_ += dt;
    const float sdt = dt * slowmo_;
    slowmo_ = Lerp(slowmo_, 1.f, dt * 0.8f);
    arena_.Update(sdt);
    boss_.Update(sdt, IdleBossInput(hero_.pos.x), arena_);
    UpdateProjectiles(sdt);
    UpdateHazards(sdt);
    particles_.Update(sdt);
    hud_.Update(dt, boss_, hero_);
    if (stateTime_ > 2.2f) FinishHeroDeath();
}

void Game::FinishHeroDeath() {
    lastTelemetry_ = tracker_.Snapshot();
    slowmo_ = 1.f;

    if (!final_ && brain_.Traits().generation >= Cfg::MAX_GENERATION) {
        state_ = State::FinalIntro;
        stateTime_ = 0.f;
        audio_.Play(AudioManager::Roar);
        glitch_ = 1.f; shake_ = 12.f;
        return;
    }
    if (brain_.Traits().generation >= Cfg::MAX_GENERATION) {
        final_ = false;
        bg_.SetEventsEnabled(true);
        state_ = State::Victory;
        stateTime_ = 0.f;
        brain_.Reset();
        audio_.Play(AudioManager::Victory);
        return;
    }

    report_ = brain_.Adapt(lastTelemetry_);
    brain_.Save();
    state_ = State::Analysis;
    stateTime_ = 0.f;
    audio_.Play(AudioManager::Analyze);
}

void Game::UpdateAnalysis(float dt) {
    stateTime_ += dt;
    arena_.Update(dt);
    boss_.Update(dt, IdleBossInput(boss_.pos.x + 100.f), arena_);
    boss_.hp = Lerp(boss_.hp, boss_.maxHp, 1.f - std::exp(-dt * 3.f));
    if (boss_.hp > boss_.maxHp - 1.f) boss_.hp = boss_.maxHp;
    particles_.Update(dt);
    hud_.Update(dt, boss_, hero_);

    if ((IsKeyPressed(KEY_ENTER) && stateTime_ > 1.5f) || stateTime_ > 14.f)
        StartRound(true);
}

void Game::UpdateFinalIntro(float dt) {
    stateTime_ += dt;
    arena_.Update(dt);
    boss_.Update(dt, IdleBossInput(boss_.pos.x + 100.f), arena_);
    boss_.hp = Lerp(boss_.hp, boss_.maxHp, 1.f - std::exp(-dt * 2.5f));
    particles_.Update(dt);
    hud_.Update(dt, boss_, hero_);
    shake_ = std::max(shake_, stateTime_ < 3.f ? 2.5f : 0.f);
    if (stateTime_ > 1.0f && GetRandomValue(0, 3) == 0)
        particles_.Burst({ Util::RandRange(60.f, 580.f), Util::RandRange(60.f, 290.f) }, 4, Color{255, 220, 120, 255}, 160.f, 2.f, 0.9f);
    if (stateTime_ > 3.6f && stateTime_ < 3.7f) { glitch_ = 1.f; audio_.Play(AudioManager::Beam, 0.8f); }
    if (stateTime_ > 4.6f) StartFinalRound();
}

void Game::StartFinalRound() {
    hero_.ApplyTraits(brain_.Traits());
    ResetRound();
    hero_.EnterFinalForm();
    boss_.SetScale(Cfg::BossScaleForGeneration(Cfg::MAX_GENERATION), true);
    final_ = true;
    director_.Start();
    bg_.SetTheme(Cfg::MAX_GENERATION);
    bg_.SetEventsEnabled(false);
    state_ = State::Playing;
    stateTime_ = 0.f;
    introTimer_ = 2.4f;
    introShrink_ = false;
    hintTimer_ = 0.f;
    audio_.Play(AudioManager::HeroSpawn, 0.8f, 0.6f);
    particles_.Ring({ hero_.pos.x, hero_.pos.y - 32.f }, 40, Color{255, 220, 120, 255}, 240.f, 3.f, 0.9f);
    glitch_ = 1.f;
}

void Game::UpdateFinalPhase(float dt) {
    director_.Update(dt, hero_, boss_, bullets_, hazards_, hero_.hp / hero_.maxHp);
    bullets_.Update(dt);

    if (director_.ConsumeTierUp()) {
        tierBannerT_ = 2.6f;
        tierBannerNum_ = director_.Tier() + 1;
        particles_.Ring(hero_.Center(), 40, Color{255, 220, 120, 255}, 300.f, 3.f, 0.8f);
        audio_.Play(AudioManager::Beam, 0.8f);
        shake_ = std::max(shake_, 10.f); glitch_ = std::max(glitch_, 0.8f);
        pickups_.SpawnCore(arena_);
        cevLabel_ = "NUCLEO DE ENERGIA: RECOGELO (CURA + SOBRECARGA)"; cevLabelT_ = 3.2f;
    }

    const BulletHit h = bullets_.Collide(boss_.HurtBox(), !boss_.IsInvulnerable());
    if (h.hit) {
        boss_.TakeDamage(h.damage);
        boss_.GrantInvuln(Cfg::BOSS_HIT_INVULN);
        particles_.Burst(h.pos, 10, Theme::NEON_RED, 180.f, 3.f, 0.4f);
        particles_.Burst(boss_.Center(), 6, WHITE, 140.f, 2.f, 0.3f);
        audio_.Play(AudioManager::Hit, 0.7f, 0.8f);
        shake_ = std::max(shake_, 4.f);
        glitch_ = std::max(glitch_, 0.5f);
        hitStop_ = std::max(hitStop_, 0.05f);
    }

    if (bullets_.ConsumeCharged() > 0) audio_.Play(AudioManager::Charge, 0.45f);
    if (bullets_.ConsumeFired() > 0) { audio_.Play(AudioManager::Beam, 0.6f); shake_ = std::max(shake_, 4.f); }
    pewT_ -= dt;
    if (director_.ConsumeBursts() > 0 && pewT_ <= 0.f) {
        audio_.Play(AudioManager::Pew, 0.22f, Util::RandRange(0.9f, 1.3f));
        pewT_ = 0.12f;
    }
}

void Game::UpdateBossDown(float dt) {
    stateTime_ += dt;
    const float sdt = dt * slowmo_;
    slowmo_ = Lerp(slowmo_, 1.f, dt * 0.8f);
    arena_.Update(sdt);
    particles_.Update(sdt);
    hero_.Update(sdt, boss_, projectiles_, arena_);
    hero_.TakeShots();
    projectiles_.clear();

    if (stateTime_ > 2.5f) {
        if (IsKeyPressed(KEY_ENTER)) { if (final_) StartFinalRound(); else StartRound(false); }
        if (IsKeyPressed(KEY_F9)) { brain_.Reset(); StartRound(false); }
    }
}

void Game::UpdateVictory(float dt) {
    stateTime_ += dt;
    arena_.Update(dt);
    particles_.Update(dt);
    if (stateTime_ > 0.2f && std::fmod(stateTime_, 0.35f) < dt)
        particles_.Burst({ Util::RandRange(80.f, 560.f), Util::RandRange(40.f, 200.f) },
                         30, (GetRandomValue(0, 1) ? Theme::NEON_CYAN : Theme::NEON_MAGENTA), 180.f, 3.f, 1.f);
    if (stateTime_ > 2.f && IsKeyPressed(KEY_ENTER)) {
        hero_.ApplyTraits(brain_.Traits());
        boss_.SetScale(1.f, true);
        bg_.SetTheme(1);
        ResetRound();
        state_ = State::Title;
        stateTime_ = 0.f;
        brainLoaded_ = false;
    }
}

void Game::DrawCenteredBanner(const char* big, const char* small, Color c) const {
    HUD::TextCentered(big, Cfg::VIEW_W / 2, 120, 40, c);
    if (small) HUD::TextCentered(small, Cfg::VIEW_W / 2, 170, 10, WHITE);
}

void Game::DrawWorld() {
    if (state_ == State::Title) { shards_.Draw(); return; }

    Camera2D cam{};
    cam.zoom = 1.f;
    cam.target = shakeOff_;
    BeginMode2D(cam);

    bg_.Draw(time_);
    arena_.Draw(time_);
    pickups_.Draw(time_);

    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& s : shockwaves_) {
        const float k = s.t / 0.4f;
        const float reach = (boss_.w * 0.5f + boss_.MeleeReach()) * std::min(1.f, k * 2.5f);
        const int a = (int)(s.origin.x), y = (int)s.origin.y;
        const Color c = Fade(Theme::NEON_MAGENTA, 1.f - k);
        for (int i = 0; i < 6; ++i)
            DrawLine(a - (int)reach, y - i * 4, a + (int)reach, y - i * 4, Fade(c, 0.5f - i * 0.07f));
        DrawCircleLines(a, y, reach, Fade(Theme::NEON_CYAN, 1.f - k));
    }
    EndBlendMode();

    hazards_.Draw(time_);
    drones_.Draw(time_);

    const bool bossVisible = !(state_ == State::BossDown && stateTime_ > 0.9f);
    if (bossVisible) boss_.Draw(sprites_, time_);
    if (hero_.Alive() && state_ != State::Victory) hero_.Draw(sprites_, time_);

    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : projectiles_) p.Draw(time_);
    particles_.Draw();
    EndBlendMode();
    bullets_.Draw(sprites_, time_);

    bg_.DrawFront(time_);
    const float lg = Clamp((1.f - Entity::worldGravity) / 0.4f, 0.f, 1.f);
    if (lg > 0.02f) {
        DrawRectangle(0, 0, Cfg::VIEW_W, Cfg::VIEW_H, Fade(Theme::NEON_CYAN, 0.05f * lg));
    }
    EndMode2D();
}

void Game::DrawOverlay() {
    const int cx = Cfg::VIEW_W / 2;
    const Telemetry tel = tracker_.Snapshot();
    const float hint = std::max(0.f, 1.f - std::max(0.f, hintTimer_ - 9.f) / 2.f);

    float evAlpha = 0.f;
    const char* evText = bg_.EventLabel(evAlpha);
    if (evAlpha > 0.01f && state_ != State::Analysis && state_ != State::Title)
        HUD::TextCentered(evText, cx, 6, 10, Fade(Theme::NEON_YELLOW, evAlpha));

    if (cevLabelT_ > 0.f && state_ == State::Playing)
        HUD::TextCentered(cevLabel_, cx, 18, 10, Fade(Theme::NEON_RED, std::min(1.f, cevLabelT_ / 0.5f)));
    if (phaseBannerT_ > 0.f && state_ == State::Playing) {
        const float a = std::min(1.f, phaseBannerT_ / 0.5f);
        HUD::TextCentered(TextFormat("FASE %d / 3", phaseBannerNum_), cx, 84, 20, Fade(Theme::NEON_RED, a));
        HUD::TextCentered("EL ANDROIDE ENTRA EN SOBRECARGA", cx, 108, 10, Fade(WHITE, a));
    }
    if (fxMsgT_ > 0.f)
        HUD::TextCentered(TextFormat("EFECTOS DE PANTALLA: %s  (F10)", FX_NAMES[fxLevel_]), cx, Cfg::VIEW_H - 28, 10,
                          Fade(Theme::NEON_CYAN, std::min(1.f, fxMsgT_)));

    hud_.SetFinal(final_, director_.PatternName(), director_.IsFlyingMode(), director_.Tier());
    if (tierBannerT_ > 0.f && state_ == State::Playing) {
        const float a = std::min(1.f, tierBannerT_ / 0.5f);
        HUD::TextCentered(TextFormat("NIVEL %d / 3", tierBannerNum_), cx, 84, 20, Fade(Color{255, 225, 120, 255}, a));
        HUD::TextCentered("EL SERAFIN DESATA TODO SU PODER", cx, 108, 10, Fade(WHITE, a));
    }

    switch (state_) {
        case State::Title: {
            MenuInfo info;
            info.fxName = FX_NAMES[fxLevel_];
            info.musicOn = audio_.MusicOn();
            info.hasMemory = brainLoaded_ || brain_.HasLearned();
            info.traits = brain_.Traits();
            info.bossScale = boss_.Scale();
            menu_.Draw(time_, info, sprites_, Util::LerpColor(Color{150, 105, 225, 255}, bg_.GridColor(), 0.35f));
            break;
        }

        case State::Playing:
            hud_.DrawBattle(boss_, hero_, tel, hint);
            if (introTimer_ > 0.f) {
                if (final_) {
                    DrawCenteredBanner("FASE FINAL", introTimer_ > 0.9f ? "EL SERAFIN DESPLIEGA SUS ALAS..." : "ESQUIVA!", Color{255, 225, 120, 255});
                } else {
                const char* sub = introTimer_ > 0.9f ? "EL ANDROIDE HA SIDO RECONSTRUIDO..." : "COMBATE!";
                DrawCenteredBanner(TextFormat("GENERACION %02d", brain_.Traits().generation), sub,
                                   introTimer_ > 0.9f ? Theme::NEON_CYAN : Theme::NEON_RED);
                }
                if (introShrink_)
                    HUD::TextCentered(TextFormat("NEMESIS-X SE COMPACTA: TAMANO %d%%",
                                      (int)(Cfg::BossScaleForGeneration(brain_.Traits().generation) * 100.f + 0.5f)),
                                      Cfg::VIEW_W / 2, 186, 10, Theme::NEON_MAGENTA);
            }
            if (paused_) {
                DrawRectangle(0, 0, Cfg::VIEW_W, Cfg::VIEW_H, Fade(BLACK, 0.6f));
                HUD::TextCentered("PAUSA", cx, 140, 40, WHITE);
                HUD::TextCentered("P: CONTINUAR    ESC: VOLVER AL MENU", cx, 190, 10, Fade(WHITE, 0.8f));
            }
            break;

        case State::HeroDying:
            hud_.DrawBattle(boss_, hero_, tel, 0.f);
            if (stateTime_ > 0.5f) HUD::TextCentered("ANDROIDE NEUTRALIZADO", cx, 120, 20, Theme::NEON_RED);
            break;

        case State::Analysis:
            hud_.DrawBattle(boss_, hero_, tel, 0.f);
            hud_.DrawAnalysis(lastTelemetry_, report_, brain_.Traits(), stateTime_);
            break;

        case State::FinalIntro: {
            const float t = stateTime_;
            DrawRectangle(0, 0, Cfg::VIEW_W, Cfg::VIEW_H, Fade(BLACK, std::min(0.78f, t * 0.4f)));
            if (t > 0.4f) HUD::TextCentered("...", cx, 100, 20, Fade(WHITE, std::min(1.f, (t - 0.4f))));
            if (t > 1.2f) HUD::TextCentered("EL ANDROIDE SE NIEGA A CAER", cx, 130, 10, Fade(WHITE, std::min(1.f, (t - 1.2f))));
            if (t > 2.2f) HUD::TextCentered("SU ENERGIA SE REUNE...", cx, 148, 10, Fade(Color{255, 225, 120, 255}, std::min(1.f, (t - 2.2f))));
            if (t > 3.3f) {
                const float j = (std::fmod(t * 30.f, 2.f) < 1.f) ? 2.f : 0.f;
                DrawText("FASE FINAL", cx - MeasureText("FASE FINAL", 40) / 2 + (int)j, 170, 40, Fade(Theme::NEON_MAGENTA, 0.8f));
                HUD::TextCentered("FASE FINAL", cx, 171, 40, Color{255, 225, 120, 255});
            }
            break;
        }

        case State::BossDown:
            hud_.DrawBattle(boss_, hero_, tel, 0.f);
            if (stateTime_ > 1.2f) {
                DrawRectangle(0, 0, Cfg::VIEW_W, Cfg::VIEW_H, Fade(BLACK, 0.6f));
                DrawCenteredBanner("FALLO DEL SISTEMA", "EL HEROE HA APRENDIDO LO SUFICIENTE: NEMESIS-X FUE DESTRUIDO", Theme::NEON_RED);
                if (stateTime_ > 2.5f && std::fmod(time_, 1.f) < 0.65f) {
                    HUD::TextCentered("[ENTER] REINTENTAR (el Heroe conserva su memoria)", cx, 200, 10, Theme::NEON_GREEN);
                    HUD::TextCentered("[F9] BORRAR MEMORIA Y REINICIAR", cx, 216, 10, Theme::NEON_CYAN);
                }
            }
            break;

        case State::Victory:
            DrawRectangle(0, 0, Cfg::VIEW_W, Cfg::VIEW_H, Fade(BLACK, 0.55f));
            DrawCenteredBanner("NEMESIS-X TRIUNFA", "EL SERAFIN HA CAIDO. EL PROTOCOLO ADAPTATIVO FUE SUPERADO.", Theme::NEON_GREEN);
            HUD::TextCentered(TextFormat("GENERACIONES DERROTADAS: %d", Cfg::MAX_GENERATION), cx, 192, 10, Theme::NEON_YELLOW);
            if (stateTime_ > 2.f && std::fmod(time_, 1.f) < 0.65f)
                HUD::TextCentered("[ENTER] VOLVER AL MENU (la memoria del Heroe se reinicia)", cx, 230, 10, WHITE);
            break;
    }
}

void Game::Draw() {
    BeginDrawing();
    fx_.BeginScene();
        ClearBackground(BLACK);
        DrawWorld();
        DrawOverlay();
    fx_.EndScene();

    ClearBackground(BLACK);
    fx_.Present(PresentRect(), time_, glitch_);
    EndDrawing();
}
