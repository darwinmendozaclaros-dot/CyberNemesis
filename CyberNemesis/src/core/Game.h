#pragma once
#include "raylib.h"
#include <vector>

#include "entities/Arena.h"
#include "entities/Boss.h"
#include "entities/Hero.h"
#include "entities/Hazards.h"
#include "entities/Drones.h"
#include "entities/Pickups.h"
#include "entities/Projectile.h"
#include "ai/PatternTracker.h"
#include "ai/AdaptiveBrain.h"
#include "ai/FinalDirector.h"
#include "entities/Bullets.h"
#include "gfx/Sprites.h"
#include "gfx/Particles.h"
#include "gfx/Background.h"
#include "gfx/PostFX.h"
#include "gfx/HUD.h"
#include "gfx/ShardField.h"
#include "gfx/MainMenu.h"
#include "audio/AudioManager.h"

class Game {
public:
    Game();
    ~Game();
    void Run();

private:
    enum class State { Title, Playing, HeroDying, Analysis, FinalIntro, BossDown, Victory };
    struct Shockwave { Vector2 origin; float t; };
    enum class CEvent { None, Bombard, Drones, LowGrav, Core };
    void StartCombatEvent(CEvent e, bool fromPhase);
    void EndCombatEvent();
    CEvent PickCombatEvent() const;

    void Init();
    void Shutdown();
    void Update(float dt);
    void Draw();

    void UpdateTitle(float dt);
    void UpdatePlaying(float dt);
    void UpdateHeroDying(float dt);
    void UpdateAnalysis(float dt);
    void UpdateFinalIntro(float dt);
    void UpdateFinalPhase(float dt);
    void StartFinalRound();
    void UpdateBossDown(float dt);
    void UpdateVictory(float dt);

    void StartRound(bool newGeneration);
    void ResetRound();
    void OnHeroDefeated();
    void OnBossDefeated();
    void FinishHeroDeath();
    void ReturnToMenu();

    BossInput ReadBossInput() const;
    BossInput IdleBossInput(float lookAtX) const;
    void HandleBossEvents();
    void HandleHeroEvents();
    void UpdateProjectiles(float dt);
    void UpdateHazards(float dt);
    void UpdateDrones(float dt);
    void UpdateCombatEvents(float dt);
    void TriggerPhase(int phase);
    Projectile MakeHeroProjectile(const ShotCmd& s);
    void SpawnExplosion(Vector2 p, Color a, Color b, int n);

    void DrawWorld();
    void DrawOverlay();
    void DrawCenteredBanner(const char* big, const char* small, Color c) const;

    Rectangle PresentRect() const;
    Vector2   MouseToView() const;

    Arena            arena_;
    Boss             boss_;
    Hero             hero_;
    Hazards          hazards_;
    DroneSwarm       drones_;
    Pickups          pickups_;
    BulletField      bullets_;
    FinalDirector    director_;
    std::vector<Projectile> projectiles_;
    std::vector<Shockwave>  shockwaves_;
    ParticleSystem   particles_;
    PatternTracker   tracker_;
    AdaptiveBrain    brain_;
    Sprites          sprites_;
    Background       bg_;
    PostFX           fx_;
    HUD              hud_;
    ShardField       shards_;
    MainMenu         menu_;
    AudioManager     audio_;

    State  state_ = State::Title;
    float  stateTime_ = 0.f, time_ = 0.f;
    float  introTimer_ = 0.f, hintTimer_ = 0.f;
    float  shake_ = 0.f, glitch_ = 0.f, hitStop_ = 0.f, slowmo_ = 1.f;
    Vector2 shakeOff_{0.f, 0.f};
    bool   paused_ = false, quit_ = false, brainLoaded_ = false;
    int    nextId_ = 1;

    int    phase_ = 0;
    CEvent cev_ = CEvent::None, lastCev_ = CEvent::None;
    float  eventTimer_ = 15.f, cevT_ = 0.f, cevDur_ = 0.f, strikeTimer_ = 0.f, gravTarget_ = 1.f;
    int    strikesLeft_ = 0;
    const char* cevLabel_ = "";
    float  cevLabelT_ = 0.f, phaseBannerT_ = 0.f;
    int    phaseBannerNum_ = 1;
    bool   introShrink_ = false, shrinkAnnounced_ = false;
    bool   final_ = false;
    float  tierBannerT_ = 0.f, pewT_ = 0.f;
    int    tierBannerNum_ = 2;
    int    fxLevel_ = 1;
    float  fxMsgT_ = 0.f;

    AdaptationReport report_;
    Telemetry        lastTelemetry_;
};
