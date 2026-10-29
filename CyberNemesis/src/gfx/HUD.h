#pragma once
#include "raylib.h"
#include "ai/AdaptiveBrain.h"

class Boss;
class Hero;

class HUD {
public:
    void Reset(const Boss& boss, const Hero& hero);
    void Update(float dt, const Boss& boss, const Hero& hero);
    void DrawBattle(const Boss& boss, const Hero& hero, const Telemetry& tel, float hintAlpha) const;
    void DrawAnalysis(const Telemetry& tel, const AdaptationReport& rep,
                      const HeroTraits& next, float t) const;

    void SetFinal(bool on, const char* pattern, bool flying, int tier) { final_ = on; pattern_ = pattern; flying_ = flying; tier_ = tier; }

    static void Text(const char* txt, int x, int y, int size, Color c);
    static void TextCentered(const char* txt, int cx, int y, int size, Color c);

private:
    static void Bar(int x, int y, int w, int h, float value, float ghost, Color fill);
    static void MiniBar(int x, int y, int w, int h, float value, Color fill);

    bool final_ = false, flying_ = false;
    const char* pattern_ = "";
    int  tier_ = 0;
    float bossShown_ = 1.f, bossGhost_ = 1.f, heroShown_ = 1.f, heroGhost_ = 1.f;
};
