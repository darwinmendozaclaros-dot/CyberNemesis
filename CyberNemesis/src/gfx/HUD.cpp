#include "gfx/HUD.h"
#include "entities/Boss.h"
#include "entities/Hero.h"
#include "core/Config.h"
#include "core/Util.h"
#include <algorithm>
#include <cmath>

void HUD::Text(const char* txt, int x, int y, int size, Color c) {
    DrawText(txt, x + 1, y + 1, size, Fade(BLACK, 0.8f));
    DrawText(txt, x, y, size, c);
}

void HUD::TextCentered(const char* txt, int cx, int y, int size, Color c) {
    Text(txt, cx - MeasureText(txt, size) / 2, y, size, c);
}

void HUD::Bar(int x, int y, int w, int h, float v, float ghost, Color fill) {
    DrawRectangle(x, y, w, h, Color{12, 10, 28, 220});
    DrawRectangle(x, y, (int)(w * Util::Clamp01(ghost)), h, Fade(WHITE, 0.55f));
    DrawRectangle(x, y, (int)(w * Util::Clamp01(v)), h, fill);
    DrawRectangleLines(x - 1, y - 1, w + 2, h + 2, Fade(fill, 0.9f));
}

void HUD::MiniBar(int x, int y, int w, int h, float v, Color fill) {
    DrawRectangle(x, y, w, h, Color{12, 10, 28, 220});
    DrawRectangle(x, y, (int)(w * Util::Clamp01(v)), h, fill);
    DrawRectangleLines(x, y, w, h, Fade(fill, 0.6f));
}

void HUD::Reset(const Boss& boss, const Hero& hero) {
    bossShown_ = bossGhost_ = boss.hp / boss.maxHp;
    heroShown_ = heroGhost_ = hero.hp / hero.maxHp;
}

void HUD::Update(float dt, const Boss& boss, const Hero& hero) {
    const float b = boss.hp / boss.maxHp;
    const float h = hero.hp / hero.maxHp;
    bossShown_ = b; heroShown_ = h;
    bossGhost_ = (bossGhost_ < b) ? b : std::max(b, bossGhost_ - dt * 0.4f);
    heroGhost_ = (heroGhost_ < h) ? h : std::max(h, heroGhost_ - dt * 0.4f);
}

void HUD::DrawBattle(const Boss& boss, const Hero& hero, const Telemetry& tel, float hintAlpha) const {
    const int W = Cfg::VIEW_W, H = Cfg::VIEW_H;

    Text("NEMESIS-X  [TU]", 10, 5, 10, Theme::NEON_MAGENTA);
    Text(TextFormat("TAM %d%%", (int)(boss.Scale() * 100.f + 0.5f)), 118, 5, 10, Fade(Theme::NEON_CYAN, 0.75f));
    Bar(10, 17, 250, 10, bossShown_, bossGhost_, Theme::NEON_MAGENTA);
    Text(TextFormat("%d", (int)boss.hp), 266, 18, 10, Fade(WHITE, 0.85f));

    Text("J", 10, 32, 10, Theme::NEON_YELLOW);
    MiniBar(22, 34, 40, 6, 1.f - boss.MeleeCooldownFrac(), Theme::NEON_YELLOW);
    Text("K", 70, 32, 10, Theme::NEON_CYAN);
    MiniBar(82, 34, 40, 6, 1.f - boss.ShotCooldownFrac(), Theme::NEON_CYAN);
    Text("SH", 130, 32, 10, Theme::NEON_GREEN);
    MiniBar(150, 34, 40, 6, 1.f - boss.DashCooldownFrac(), Theme::NEON_GREEN);
    DrawCircle(204, 38, 3.f, boss.AirJumpAvailable() ? Theme::NEON_GREEN : Fade(GRAY, 0.4f));
    if (boss.IsOvercharged() && std::fmod(GetTime() * 6.0, 1.0) < 0.75)
        Text("SOBRECARGA", 214, 32, 10, Theme::NEON_YELLOW);

    const HeroTraits& t = hero.Traits();
    const int hx = W - 10 - 250;
    if (final_) Text("SERAFIN - FORMA FINAL", hx, 5, 10, Color{255, 225, 120, 255});
    else        Text(TextFormat("ANDROIDE  GEN %02d", t.generation), hx, 5, 10, Theme::NEON_CYAN);
    Bar(hx, 17, 250, 10, heroShown_, heroGhost_, Theme::NEON_CYAN);
    DrawRectangle(hx + (int)(250 * Cfg::PHASE1_FRAC), 16, 1, 12, Fade(WHITE, 0.75f));
    DrawRectangle(hx + (int)(250 * Cfg::PHASE2_FRAC), 16, 1, 12, Fade(WHITE, 0.75f));
    Text(TextFormat("%d", (int)std::max(0.f, hero.hp)), hx - 28, 18, 10, Fade(WHITE, 0.85f));

    if (final_) {
        Text(TextFormat("%s  NIVEL %d/3", flying_ ? "VUELO" : "TIERRA", tier_ + 1), hx, 31, 10, Fade(WHITE, 0.85f));
        Text(TextFormat("ATAQUE: %s", pattern_), hx, 44, 10, Theme::NEON_YELLOW);
    }
    int tx = hx;
    auto pill = [&](int y, const char* label, bool on, Color c) {
        int w = MeasureText(label, 10) + 8;
        DrawRectangle(tx, y, w, 12, on ? Fade(c, 0.25f) : Fade(GRAY, 0.12f));
        DrawRectangleLines(tx, y, w, 12, on ? c : Fade(GRAY, 0.35f));
        DrawText(label, tx + 4, y + 2, 10, on ? c : Fade(GRAY, 0.5f));
        tx += w + 4;
    };
    if (!final_) {
    pill(31, "ESCUDO", t.deflectShield, Theme::NEON_CYAN);
    pill(31, "DASH", t.dashEvasive, Theme::NEON_YELLOW);
    pill(31, "PREDICCION", t.leadShooting, Theme::NEON_GREEN);
    pill(31, "JETS", t.jumpJets, Theme::NEON_MAGENTA);
    tx = hx;
    pill(46, "RAF", hero.Has(Atk::BURST), Theme::NEON_CYAN);
    pill(46, "ESC", hero.Has(Atk::SPREAD), Theme::NEON_GREEN);
    pill(46, "MIS", hero.Has(Atk::MISSILE), Theme::NEON_YELLOW);
    pill(46, "ORB", hero.Has(Atk::ORBITAL), Theme::NEON_RED);
    pill(46, "MIN", hero.Has(Atk::MINE), Theme::NEON_MAGENTA);
    if (t.dashEvasive) MiniBar(hx, 61, 60, 4, 1.f - hero.DashCooldownFrac(), Theme::NEON_YELLOW);
    }

    const int px = W - 168, py = H - 52;
    DrawRectangle(px - 4, py - 4, 168, 50, Fade(BLACK, 0.45f));
    Text("IA: TELEMETRIA EN VIVO", px, py, 10, Fade(Theme::NEON_CYAN, 0.85f));
    if (tel.TotalAttacks() > 0)
        Text(TextFormat("DIST %d%%  MELEE %d%%", (int)(tel.RangedRatio() * 100.f),
                        (int)(tel.MeleeRatio() * 100.f)), px, py + 11, 10, Fade(WHITE, 0.8f));
    else
        Text("DIST --  MELEE --", px, py + 11, 10, Fade(WHITE, 0.6f));
    Text(TextFormat("PREDECIBLE %d%%", (int)(tel.predictability * 100.f)), px, py + 22, 10,
         tel.predictability > Cfg::PREDICT_THRESHOLD ? Theme::NEON_RED : Fade(WHITE, 0.8f));
    Text(TextFormat("MOVILIDAD %d%%", (int)(std::min(1.f, tel.Mobility()) * 100.f)), px, py + 33, 10,
         tel.Mobility() > Cfg::MOBILITY_THRESHOLD ? Theme::NEON_RED : Fade(WHITE, 0.8f));

    if (hintAlpha > 0.f) {
        Text(final_ ? "ESQUIVA LAS BALAS: SHIFT = DASH INVULNERABLE   J = APLASTAR (BORRA BALAS)   K/CLIC = DISPARAR"
                    : "A/D MOVER  W SALTAR (x2)  SHIFT DASH  S BAJAR  J APLASTAR (en el aire: PICADO)  K/CLIC DISPARAR", 10, H - 14, 10,
             Fade(WHITE, hintAlpha));
    }
}

void HUD::DrawAnalysis(const Telemetry& tel, const AdaptationReport& rep,
                       const HeroTraits& next, float t) const {
    const int W = Cfg::VIEW_W, H = Cfg::VIEW_H;
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.86f));
    for (int y = 0; y < H; y += 4) DrawRectangle(0, y, W, 1, Fade(Theme::NEON_CYAN, 0.04f));

    TextCentered("// ANDROIDE DESTRUIDO //", W / 2, 14, 20, Theme::NEON_RED);
    TextCentered("PROCESANDO TELEMETRIA DEL COMBATE...", W / 2, 40, 10, Theme::NEON_CYAN);

    const float p = Util::Clamp01(t / 1.0f);
    const struct { const char* label; float value; float thr; bool needData; } rows[4] = {
        { "ATAQUES A DISTANCIA",     rep.rangedRatio,     Cfg::RATIO_THRESHOLD,    true  },
        { "ATAQUES CUERPO A CUERPO", rep.meleeRatio,      Cfg::RATIO_THRESHOLD,    true  },
        { "MOVIMIENTO PREDECIBLE",   rep.predictability,  Cfg::PREDICT_THRESHOLD,  false },
        { "MOVILIDAD / ALTURA",      std::min(1.f, rep.mobility), Cfg::MOBILITY_THRESHOLD, false },
    };
    for (int i = 0; i < 4; ++i) {
        const int y = 60 + i * 21;
        const bool over = rows[i].value > rows[i].thr && (rep.enoughData || !rows[i].needData);
        const Color c = over ? Theme::NEON_RED : Theme::NEON_CYAN;
        Text(rows[i].label, 70, y, 10, WHITE);
        const int bx = 250, bw = 230;
        DrawRectangle(bx, y, bw, 10, Color{12, 10, 28, 255});
        DrawRectangle(bx, y, (int)(bw * rows[i].value * p), 10, c);
        DrawRectangleLines(bx - 1, y - 1, bw + 2, 12, Fade(c, 0.8f));
        const int tx = bx + (int)(bw * rows[i].thr);
        DrawRectangle(tx, y - 3, 2, 16, Theme::NEON_YELLOW);
        Text(TextFormat("%d%%", (int)(rows[i].value * 100.f * p)), bx + bw + 10, y, 10, c);
    }
    if (!rep.enoughData)
        Text("(pocos ataques registrados: la muestra de ataques no es concluyente)", 70, 146, 10, Fade(WHITE, 0.55f));

    Text("CONTRAMEDIDAS Y EVOLUCION:", 70, 162, 10, Theme::NEON_YELLOW);
    const int shown = std::max(0, (int)((t - 1.1f) / 0.5f) + 1);
    const int total = (int)rep.lines.size();
    for (int i = 0; i < total && i < shown && i < 8; ++i)
        Text(rep.lines[i].c_str(), 78, 177 + i * 12, 10, i + 1 == total ? Fade(WHITE, 0.7f) : Theme::NEON_GREEN);

    TextCentered(TextFormat("GEN %02d  ->  GEN %02d     HP %d    DANO %.1f    CADENCIA %.2fs",
                            rep.fromGen, rep.toGen, (int)next.maxHp, next.damage, next.fireInterval),
                 W / 2, 292, 10, Theme::NEON_CYAN);
    TextCentered("NEMESIS-X SE REGENERA POR COMPLETO...", W / 2, 306, 10, Theme::NEON_MAGENTA);
    if (t > 1.5f && std::fmod(t, 1.0f) < 0.65f)
        TextCentered("[ENTER] ENFRENTAR A LA NUEVA GENERACION", W / 2, 330, 10, WHITE);
    (void)tel;
}
