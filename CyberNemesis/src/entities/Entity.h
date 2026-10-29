#pragma once
#include "raylib.h"
#include "raymath.h"
#include "core/Config.h"

class Arena;

class Entity {
public:
    Entity(float width, float height) : w(width), h(height) {}
    virtual ~Entity() = default;

    Rectangle Bounds() const { return { pos.x - w * 0.5f, pos.y - h, w, h }; }
    Vector2   Center() const { return { pos.x, pos.y - h * 0.5f }; }
    bool      OnGround() const { return grounded_; }
    int       StandingPlatform() const { return standIdx_; }
    bool      Alive() const { return hp > 0.f; }

    void DropThrough(float t = 0.25f) {
        if (standIdx_ >= 0) { dropT_ = t; standIdx_ = -1; grounded_ = false; }
    }

    static inline float worldGravity = 1.f;

    Vector2 pos{0.f, 0.f};
    Vector2 vel{0.f, 0.f};
    float   w, h;
    float   hp = 1.f, maxHp = 1.f;
    int     facing = 1;
    float   hitFlash = 0.f;
    float   animTime = 0.f;
    float   walkPhase = 0.f;

    float   LandSquash() const { return landSquash_; }

protected:
    void ResetPhysics() { grounded_ = true; standIdx_ = -1; dropT_ = 0.f; }
    void ApplyPhysics(float dt, const Arena& arena, float gravityScale = 1.f);

    float landSquash_ = 0.f;
    bool  wasGrounded_ = true;
    bool  grounded_ = true;
    int   standIdx_ = -1;
    float dropT_    = 0.f;
};
