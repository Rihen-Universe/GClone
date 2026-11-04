// Nk25DGame.hpp — Gameplay "slots" : stock de blocs numérotés -> 5 emplacements -> fourmis -> image.
// Sans STL. Entrées : OnTap(x,y) en pixels écran. Rendu : DrawBackground() puis scene.Render() puis DrawUI().
#pragma once
#include "Nk25DBoard.h"

namespace nk25d {

// ---- Primitives UI (espace écran) ---------------------------------------------
inline void RoundRect(IPainter& p, float cx, float cy, float w, float h, float r, Col c) {
    Vec2 pts[24]; int n = 0; const int S = 5;
    r = m::Min(r, m::Min(w, h) * .499f); float hx = w * .5f - r, hy = h * .5f - r;
    const float ox[4] = {hx, -hx, -hx, hx}, oy[4] = {hy, hy, -hy, -hy}, a0[4] = {0, PI * .5f, PI, PI * 1.5f};
    for (int k = 0; k < 4; ++k) for (int s = 0; s <= S; ++s) {
        float a = a0[k] + PI * .5f * s / S; pts[n++] = { cx + ox[k] + m::Cos(a) * r, cy + oy[k] + m::Sin(a) * r };
    }
    p.FillPolygon(pts, n, c);
}
inline Col Mix(Col a, Col b, float t) {
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t), (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}
inline void UiText(IPainter& p, float x, float y, float size, const char* t, Col c, bool shadow = true) {
    if (shadow) p.DrawText({x + size * .05f, y + size * .08f}, t, size, {0, 0, 0, 90});
    p.DrawText({x, y}, t, size, c);
}
// Bloc "bonbon" 3D : lèvre sombre, face, reflet, contour blanc si sélectionnable
inline void Block3D(IPainter& p, float cx, float cy, float sz, Col c, const char* txt, bool dim, bool outline, bool hidden) {
    Col base = hidden ? Col{150, 105, 200, 255} : c;
    if (dim) base = Mix(base, {205, 200, 205, 255}, 0.55f);
    if (outline) RoundRect(p, cx, cy + sz * .02f, sz + 7, sz + 7, sz * .25f, {255, 255, 255, 255});
    RoundRect(p, cx, cy + sz * .07f, sz, sz, sz * .22f, Shade(base, 0.66f));
    RoundRect(p, cx, cy - sz * .02f, sz, sz * .93f, sz * .22f, base);
    RoundRect(p, cx, cy - sz * .13f, sz * .84f, sz * .6f, sz * .17f, Shade(base, 1.13f));
    int lum = (base.r * 3 + base.g * 6 + base.b) / 10;
    Col tc = lum > 185 ? Col{70, 70, 85, 255} : Col{255, 255, 255, 255};
    if (dim) tc.a = 170;
    UiText(p, cx, cy - sz * .04f, sz * .42f, txt, tc, lum <= 185);
}

struct StockBlock { int color; int count; bool mystery; };
struct Slot { bool active = false; int color = -1, remaining = 0, outstanding = 0; float spawnCd = 0; };

class Gameplay {
public:
    enum State { PLAYING, WON, LOST };
    enum { MAXSLOTS = 7, COLS = 4, MAXCOL = 64, MAXANTS = 80, MAXALL = 256 };
    int level = 31;

    void Init(Scene* sc, Board* b, float W, float H) {
        sc_ = sc; board_ = b; W_ = W; H_ = H; na_ = 0;
        sc->camera.tilt = 0.85f; sc->camera.origin = {0, 0};
        float tilt = sc->camera.tilt;
        b->cs = m::Max(4.f, (float)(int)(W * 0.86f / b->gw)); b->ch = b->cs * 0.8f;
        b->ox = (W - b->gw * b->cs) * .5f; b->oy = H * 0.135f / tilt;
        hx_ = W * .5f; hy_ = H * 0.585f / tilt;
        Hole* h = sc->Add<Hole>(); h->x = hx_; h->y = hy_; h->rx = W * 0.055f; h->ry = h->rx * 0.65f;
        antScale_ = b->cs / 9.f; nSlots_ = 5; rng_ = 1234567u + (unsigned)level * 7919u;
        BuildStock();
    }

    void AutoPlay(bool on) { auto_ = on; }
    State GetState() const { return state_; }
    float TimeScale() const { return speedMul_; }
    int   StockLeft() const { int t = 0; for (int c = 0; c < COLS; ++c) t += len_[c]; return t; }

    // Avance le jeu (sous-pas <= 1/30 s pour rester stable à vitesse x3)
    void Tick(float dt) {
        float d = dt * speedMul_;
        while (d > 1e-5f) { float s = m::Min(d, 1.f / 30.f); Step(s); d -= s; }
    }

    // ---- Entrées ---------------------------------------------------------------
    void OnTap(float x, float y) {
        if (state_ != PLAYING) return;
        if (Hit(x, y, W_ * .87f, H_ * .047f, W_ * .08f, H_ * .025f)) { speedMul_ = speedMul_ >= 3.f ? 1.f : speedMul_ + 1.f; return; }
        for (int i = 0; i < 4; ++i)
            if (m::Hypot(x - BtnX(i), y - BtnY()) < W_ * .085f) { Power(i); return; }
        for (int c = 0; c < COLS; ++c) for (int r = 0; r < 3 && r < len_[c]; ++r) {
            if (r > 0 && !hand_) continue;
            if (Hit(x, y, StockX(c), StockY(r), TileSz() * .5f + 4, TileSz() * .5f + 4)) { Deploy(c, r); return; }
        }
    }

    // ---- Rendu -----------------------------------------------------------------
    void DrawBackground(IPainter& p) const {                     // plateau clair derrière l'image
        float x0 = board_->ox - 16, y0 = (board_->oy) * sc_->camera.tilt - 14;
        float w = board_->gw * board_->cs + 32, h = board_->gh * board_->cs * sc_->camera.tilt + board_->ch + 30;
        RoundRect(p, x0 + w * .5f, y0 + h * .5f + 3, w, h, 18, {170, 140, 110, 90});
        RoundRect(p, x0 + w * .5f, y0 + h * .5f, w, h, 18, {240, 228, 214, 255});
        RoundRect(p, x0 + w * .5f, y0 + h * .5f, w - 8, h - 8, 14, {232, 218, 204, 255});
    }

    void DrawUI(IPainter& p) const {
        char buf[24];
        // --- stock (colonnes) : du fond vers l'avant
        for (int c = 0; c < COLS; ++c) for (int r = m::Min(2, len_[c] - 1); r >= 0; --r) {
            const StockBlock& b = col_[c][r]; bool hidden = b.mystery && r > 0 && !hand_;
            if (hidden) { buf[0] = '?'; buf[1] = 0; } else IntToStr(b.count, buf);
            Block3D(p, StockX(c), StockY(r), TileSz(), board_->palette[b.color], buf, r > 0 && !hand_, r == 0 || hand_, hidden);
        }
        // --- barre basse + boutons de pouvoir
        RoundRect(p, W_ * .5f, H_ * 1.0f, W_ * 1.06f, H_ * .11f, 6, {150, 60, 205, 255});
        const Col bc[4] = {{110, 200, 90, 255}, {240, 190, 50, 255}, {120, 130, 150, 255}, {70, 150, 230, 255}};
        const char* lab[4] = {"+", "H", "C", "U"};
        for (int i = 0; i < 4; ++i) {
            float x = BtnX(i), y = BtnY(), r = W_ * .075f;
            p.FillEllipse({x, y}, r + 4, r + 4, {150, 60, 205, 255}); p.FillEllipse({x, y}, r, r, {255, 240, 235, 255});
            p.FillEllipse({x, y + 2}, r * .78f, r * .78f, Shade(bc[i], .85f)); p.FillEllipse({x, y}, r * .78f, r * .78f, bc[i]);
            UiText(p, x, y, r * 1.1f, lab[i], {255, 255, 255, 255});
            float bx = x + r * .8f, by = y + r * .8f;
            if (pw_[i] > 0) { p.FillEllipse({bx, by}, r * .36f, r * .36f, {255, 170, 30, 255}); IntToStr(pw_[i], buf); UiText(p, bx, by, r * .5f, buf, {255,255,255,255}, false); }
            else { p.FillEllipse({bx, by}, r * .36f, r * .36f, {70, 200, 70, 255}); UiText(p, bx, by, r * .6f, "+", {255,255,255,255}, false); }
        }
        // --- slots
        int n = nSlots_; float sp = m::Min(W_ * .16f, W_ * .88f / n), x0 = W_ * .5f - sp * (n - 1) * .5f, ssz = W_ * .125f;
        for (int i = 0; i < n; ++i) {
            float x = x0 + sp * i, y = H_ * .685f;
            if (slot_[i].active) { IntToStr(slot_[i].remaining, buf); Block3D(p, x, y, ssz, board_->palette[slot_[i].color], buf, false, false, false); }
            else { RoundRect(p, x, y + 5, ssz, ssz, ssz * .2f, {200, 175, 150, 255}); RoundRect(p, x, y, ssz, ssz * .94f, ssz * .2f, {243, 239, 235, 255}); }
        }
        // --- en-tête
        p.FillEllipse({W_ * .075f, H_ * .047f}, W_ * .055f, W_ * .055f, {255, 255, 255, 255});
        p.FillEllipse({W_ * .075f, H_ * .047f}, W_ * .046f, W_ * .046f, {150, 60, 205, 255});
        IntToStr(level, buf); char lv[32] = "Level "; int k = 6; for (int i = 0; buf[i]; ++i) lv[k++] = buf[i]; lv[k] = 0;
        RoundRect(p, W_ * .5f, H_ * .047f, W_ * .27f, H_ * .033f, H_ * .016f, {150, 60, 205, 255});
        UiText(p, W_ * .5f, H_ * .047f, H_ * .021f, lv, {255, 255, 255, 255});
        RoundRect(p, W_ * .87f, H_ * .047f, W_ * .16f, H_ * .04f, H_ * .02f, {120, 120, 125, 255});
        buf[0] = 'x'; IntToStr((int)speedMul_, buf + 1); UiText(p, W_ * .87f, H_ * .047f, H_ * .022f, buf, {255, 255, 255, 255});
        if (hand_) UiText(p, W_ * .5f, H_ * .74f - TileSz() * .8f, H_ * .02f, "Choisis un bloc", {255, 255, 255, 255});
        if (boost_ > 0) UiText(p, W_ * .5f, H_ * .1f, H_ * .02f, "UFO !", {255, 240, 120, 255});
        if (state_ != PLAYING) {
            RoundRect(p, W_ * .5f, H_ * .4f, W_ * .8f, H_ * .14f, 24, {40, 20, 60, 215});
            UiText(p, W_ * .5f, H_ * .385f, H_ * .04f, state_ == WON ? "Niveau termine !" : "Echec", {255, 255, 255, 255});
        }
    }

private:
    // ---- mise en page ---------------------------------------------------------------
    float TileSz() const { return W_ * .118f; }
    float StockX(int c) const { return W_ * (0.26f + 0.16f * c); }
    float StockY(int r) const { return H_ * (0.765f + 0.07f * r); }
    float BtnX(int i) const { return W_ * (0.176f + 0.217f * i); }
    float BtnY() const { return H_ * .955f; }
    static bool Hit(float x, float y, float cx, float cy, float hw, float hh) { return x >= cx - hw && x <= cx + hw && y >= cy - hh && y <= cy + hh; }
    unsigned Rnd() { rng_ = rng_ * 1664525u + 1013904223u; return rng_ >> 16; }

    // ---- stock -----------------------------------------------------------------------
    void BuildStock() {
        StockBlock all[MAXALL]; int n = 0; static const int sz[7] = {10, 15, 16, 20, 30, 40, 50};
        for (int c = 0; c < board_->np; ++c) {
            int left = board_->Remaining(c);
            while (left > 0 && n < MAXALL) {
                int cand[7], nc = 0; for (int i = 0; i < 7; ++i) if (sz[i] <= left) cand[nc++] = sz[i];
                int s = nc ? cand[Rnd() % nc] : left; if (n == MAXALL - 1) s = left;
                all[n++] = { c, s, (Rnd() % 100) < 18 }; left -= s;
            }
        }
        for (int i = n - 1; i > 0; --i) { int j = Rnd() % (i + 1); StockBlock t = all[i]; all[i] = all[j]; all[j] = t; }
        for (int c = 0; c < COLS; ++c) len_[c] = 0;
        for (int i = 0; i < n; ++i) { int c = i % COLS; if (len_[c] < MAXCOL) col_[c][len_[c]++] = all[i]; }
    }
    bool FreeSlot(int& out) const { for (int i = 0; i < nSlots_; ++i) if (!slot_[i].active) { out = i; return true; } return false; }
    void Deploy(int c, int r) {
        int si; if (!FreeSlot(si)) return;
        StockBlock b = col_[c][r];
        for (int k = r; k + 1 < len_[c]; ++k) col_[c][k] = col_[c][k + 1];
        --len_[c];
        slot_[si].active = true; slot_[si].color = b.color; slot_[si].remaining = b.count; slot_[si].outstanding = 0; slot_[si].spawnCd = 0.1f;
        hand_ = false;
    }
    void Power(int i) {
        if (pw_[i] <= 0) { pw_[i] = 1; return; }                 // bouton "+" : place de la pub / récompense
        if (i == 0) { if (nSlots_ >= MAXSLOTS) return; --pw_[0]; ++nSlots_; }
        else if (i == 1) { --pw_[1]; hand_ = true; }
        else if (i == 2) { --pw_[2]; Cannon(); }
        else { --pw_[3]; boost_ = 15.f; }
    }
    void Cannon() {                                              // retire 15 blocs exposés de la couleur la plus exposée
        int ex[MAX_PAL]; board_->ExposedCounts(ex); int bc = 0;
        for (int c = 1; c < board_->np; ++c) if (ex[c] > ex[bc]) bc = c;
        for (int k = 0; k < 15; ++k) { int cell = board_->FindNearest(bc, hx_, hy_); if (cell < 0) break; board_->Take(cell); }
        board_->UpdateReach();
    }

    // ---- boucle de jeu ---------------------------------------------------------------
    void Step(float dt) {
        sc_->Update(dt);
        if (state_ != PLAYING) return;
        if (boost_ > 0) boost_ -= dt;
        if ((expoT_ -= dt) <= 0) { board_->UpdateReach(); board_->ExposedCounts(expo_); expoT_ = 0.2f; }
        int tasks[MAXSLOTS] = {}; bool busy = false;
        for (int i = 0; i < na_; ++i) { if (ants_[i]->task >= 0) { ++tasks[ants_[i]->owner]; busy = true; } if (ants_[i]->Busy()) busy = true; }

        for (int s = 0; s < nSlots_; ++s) {                       // vidage / libération / apparition
            Slot& sl = slot_[s]; if (!sl.active) continue;
            if (sl.remaining > 0 && board_->Remaining(sl.color) == 0) sl.remaining = 0;     // couleur épuisée
            if (sl.remaining <= 0 && sl.outstanding == 0) { sl.active = false; continue; }
            sl.spawnCd -= dt;
            int maxAnts = 6 + (boost_ > 0 ? 4 : 0);
            if (sl.spawnCd <= 0 && sl.remaining - tasks[s] > 0 && sl.outstanding < maxAnts && expo_[sl.color] > 0 && na_ < MAXANTS) {
                SpawnAnt(s); sl.spawnCd = boost_ > 0 ? 0.12f : 0.2f;      // la nouvelle fourmi prendra sa tâche dans la boucle suivante
            }
        }
        for (int i = 0; i < na_; ++i) {
            Ant* a = ants_[i];
            if (a->Busy()) { if (a->Stuck() >= 2) { PathHome(a); a->ResetStuck(); } continue; }   // chargée et coincée : on recalcule le chemin
            Slot& sl = slot_[a->owner];
            if (a->task == -2) {
                if (m::Hypot(a->x - hx_, a->y - hy_) < 16.f) { Despawn(i); --i; } else if (a->Arrived()) PathHome(a);
                continue;
            }
            if (a->task >= 0) {
                if (!board_->Present(a->task, a->colorIdx)) { a->task = -1; --tasks[a->owner]; }
                else {
                    float qx, qy; board_->ClosestOnCell(a->task, a->x, a->y, qx, qy);
                    if (m::Hypot(qx - a->x, qy - a->y) <= a->Radius() + 3.f) { --tasks[a->owner]; Grab(a); busy = true; continue; }
                    if (a->Stuck() >= 3) { board_->Cooldown(a->task, 10.f); a->task = -1; --tasks[a->owner]; a->ResetStuck(); }
                    else {
                        if (a->Arrived() && !PathToCell(a)) { board_->Cooldown(a->task, 10.f); a->task = -1; --tasks[a->owner]; }
                        else continue;
                    }
                }
            }
            // sans tâche : un autre bloc si le slot en a encore besoin, sinon retour au trou
            int cell = -1;
            if (sl.remaining - tasks[a->owner] > 0 && expo_[a->colorIdx] > 0) cell = board_->FindNearest(a->colorIdx, a->x, a->y);
            if (cell >= 0) {
                a->task = cell; ++tasks[a->owner]; a->ResetStuck(); board_->Cooldown(cell, 1e6f);
                if (!PathToCell(a)) { board_->Cooldown(cell, 10.f); a->task = -1; --tasks[a->owner]; }     // aucun chemin : on change plus tard
            } else { a->task = -2; PathHome(a); }
        }
        // victoire / défaite
        if (board_->Remaining() == 0 && !busy) { state_ = WON; return; }
        bool progress = busy, freeSlot = false; int fs; freeSlot = FreeSlot(fs) && StockLeft() > 0;
        for (int s = 0; s < nSlots_; ++s) if (slot_[s].active && slot_[s].remaining > 0 && expo_[slot_[s].color] > 0) progress = true;
        if (!progress && !freeSlot) { if ((stallT_ += dt) > 1.5f) state_ = LOST; } else stallT_ = 0;
        if (auto_) { if ((autoT_ -= dt) <= 0) { autoT_ = 0.9f; AutoPick(); } }
    }

    void AutoPick() {                                            // bot de test : couleur accessible, petit bloc d'abord
        int fs; if (!FreeSlot(fs)) return; int bestc = -1; int bs = -1000000;
        for (int c = 0; c < COLS; ++c) if (len_[c] > 0) {
            int col = col_[c][0].color; if (expo_[col] <= 0) continue;
            int sc = expo_[col] * 10 - col_[c][0].count; if (sc > bs) { bs = sc; bestc = c; }
        }
        if (bestc < 0) {                                         // aucune couleur accessible : on débloque quelque chose seulement si rien n'avance
            bool prog = false; for (int s = 0; s < nSlots_; ++s) if (slot_[s].active && slot_[s].remaining > 0 && expo_[slot_[s].color] > 0) prog = true;
            if (prog) return; for (int c = 0; c < COLS; ++c) if (len_[c] > 0) { bestc = c; break; }
        }
        if (bestc >= 0) Deploy(bestc, 0);
    }
    void SpawnAnt(int s) {
        Ant* a = sc_->Add<Ant>(); a->colorIdx = slot_[s].color; a->owner = s; a->color = board_->palette[a->colorIdx];
        a->scale = antScale_; a->eatOnArrive = true; a->arriveRadius = 16.f; a->heading = -PI * .5f;
        a->speed = 120.f + float(Rnd() % 40); a->x = hx_ + float(Rnd() % 21) - 10; a->y = hy_ + float(Rnd() % 11) - 5;
        ants_[na_++] = a; ++slot_[s].outstanding;
    }
    void Despawn(int i) { Ant* a = ants_[i]; a->dead = true; --slot_[a->owner].outstanding; ants_[i] = ants_[--na_]; }
    void Grab(Ant* a) {
        int cell = a->task; float cx = board_->CellX(cell), cy = board_->CellY(cell);
        int col = board_->Take(cell); --slot_[a->owner].remaining;
        Box* b = sc_->Add<Box>(); b->w = b->d = board_->cs; b->h = board_->ch; b->color = board_->palette[col];
        b->showText = false; b->carryScale = 0.8f; b->x = cx; b->y = cy;
        a->task = -1; a->PickUp(b, hx_, hy_); PathHome(a);
    }
    bool PathToCell(Ant* a) {                                    // chemin jusqu'au bloc visé
        Vec2 pts[40]; int n = board_->FindPathToCell(a->x, a->y, a->task, pts, 40); if (n <= 0) return false;
        a->arriveRadius = 5.f; a->SetPath(pts, n, board_->CellX(a->task), board_->CellY(a->task)); return true;
    }
    void PathHome(Ant* a) {                                      // chemin jusqu'au trou
        Vec2 pts[40]; int n = board_->FindPathToPoint(a->x, a->y, hx_, hy_, pts, 40); a->arriveRadius = 16.f;
        if (n > 0) a->SetPath(pts, n, hx_, hy_); else a->SetTarget(hx_, hy_);
    }

    Scene* sc_ = nullptr; Board* board_ = nullptr; float W_ = 540, H_ = 960, hx_ = 0, hy_ = 0, antScale_ = 1;
    StockBlock col_[COLS][MAXCOL]; int len_[COLS] = {};
    Slot slot_[MAXSLOTS]; int nSlots_ = 5;
    Ant* ants_[MAXANTS]; int na_ = 0;
    int pw_[4] = {1, 0, 0, 1}; bool hand_ = false; float boost_ = 0, speedMul_ = 1;
    int expo_[MAX_PAL] = {}; float expoT_ = 0, stallT_ = 0, autoT_ = 1.0f; bool auto_ = false;
    State state_ = PLAYING; unsigned rng_ = 1;
};

} // namespace nk25d