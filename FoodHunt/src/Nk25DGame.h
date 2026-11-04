// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DGame.h — Gameplay "slots" : stock de blocs numérotés -> 5 emplacements -> fourmis -> image.
// Migré le 04/11/2026 sur Nkentseu. Entrées : OnTap(x,y) en pixels écran. Rendu : DrawBackground() puis
// scene.Render() puis DrawUI(), tous sur le même NkGuiDrawList.
#pragma once
#include "Nk25DBoard.h"

namespace nk25d {

// ---- Primitives UI (espace écran) ---------------------------------------------
// Texte centré avec ombre : le texte de l'interface passe par NkTexteCentreTaille (police embarquée de NKFont).
inline void UiText(Dl& dl, float x, float y, float size, const char* t, const NkColor& c, bool shadow = true) {
    GameText(dl, x, y, size, t, c, shadow);
}

// Rectangle arrondi centré en (cx, cy) : le rayon est borné à la moitié du petit côté (au-delà, ce serait un disque).
inline void RoundRectC(Dl& dl, float cx, float cy, float w, float h, float r, const NkColor& c) {
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h), c, NkMin(r, NkMin(w, h) * .499f));
}

// Bloc "bonbon" 3D de l'interface : lèvre sombre, face, reflet, contour blanc si sélectionnable.
// C'est un faux relief fait de TROIS rectangles arrondis superposés, du dessous vers le dessus :
//   1. la « lèvre » (teinte x 0.66), décalée vers le BAS de 7 % : elle figure l'épaisseur latérale du bloc,
//      vue d'un peu au-dessus (le même principe que la caméra 2.5D, avec une inclinaison fixe) ;
//   2. la face avant, de la teinte de base, un peu plus courte que la lèvre : elle laisse voir la lèvre en bas ;
//   3. le reflet : un rectangle plus petit et plus clair (x 1.13) vers le haut, qui suggère une surface bombée
//      éclairée d'en haut.
// La couleur du texte dépend de la LUMINANCE de la face, lum = (3 R + 6 G + B) / 10 (approximation entière de
// 0.299 R + 0.587 G + 0.114 B, formule de la Rec. 601) : texte sombre sur fond clair, blanc sur fond foncé,
// pour garder un contraste lisible quelle que soit la couleur du bloc.
inline void Block3D(Dl& dl, float cx, float cy, float sz, NkColor c, const char* txt, bool dim, bool outline, bool hidden) {
    NkColor base = hidden ? NkColor(150, 105, 200, 255) : c;
    if (dim) base = base.Lerp(NkColor(205, 200, 205, 255), 0.55f);
    if (outline) RoundRectC(dl, cx, cy + sz * .02f, sz + 7, sz + 7, sz * .25f, NkColor(255, 255, 255, 255));
    RoundRectC(dl, cx, cy + sz * .07f, sz, sz, sz * .22f, base.Shade(0.66f));
    RoundRectC(dl, cx, cy - sz * .02f, sz, sz * .93f, sz * .22f, base);
    RoundRectC(dl, cx, cy - sz * .13f, sz * .84f, sz * .6f, sz * .17f, base.Shade(1.13f));
    int lum = (base.r * 3 + base.g * 6 + base.b) / 10;
    NkColor tc = lum > 185 ? NkColor(70, 70, 85, 255) : NkColor(255, 255, 255, 255);
    if (dim) tc.a = 170;
    UiText(dl, cx, cy - sz * .04f, sz * .42f, txt, tc, lum <= 185);
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
        sc->camera.tilt = 0.85f; sc->camera.origin = NkVec2f(0.f, 0.f);
        float tilt = sc->camera.tilt;
        b->cs = NkMax(4.f, (float)(int)(W * 0.86f / b->gw)); b->ch = b->cs * 0.8f;
        b->ox = (W - b->gw * b->cs) * .5f; b->oy = H * 0.135f / tilt;
        hx_ = W * .5f; hy_ = H * 0.585f / tilt;
        Hole* h = sc->Add<Hole>(); h->x = hx_; h->y = hy_; h->rx = W * 0.055f; h->ry = h->rx * 0.65f;
        antScale_ = b->cs / 9.f; nSlots_ = 5; rng_.Seed(1234567u + (uint32)level * 7919u);
        BuildStock();
    }

    void AutoPlay(bool on) { auto_ = on; }
    State GetState() const { return state_; }
    float TimeScale() const { return speedMul_; }
    int   StockLeft() const { int t = 0; for (int c = 0; c < COLS; ++c) t += len_[c]; return t; }

    // Avance le jeu (sous-pas <= 1/30 s pour rester stable à vitesse x3)
    void Tick(float dt) {
        float d = dt * speedMul_;
        while (d > 1e-5f) { float s = NkMin(d, 1.f / 30.f); Step(s); d -= s; }
    }

    // ---- Entrées ---------------------------------------------------------------
    void OnTap(float x, float y) {
        if (state_ != PLAYING) return;
        if (Hit(x, y, W_ * .87f, H_ * .047f, W_ * .08f, H_ * .025f)) { speedMul_ = speedMul_ >= 3.f ? 1.f : speedMul_ + 1.f; return; }
        for (int i = 0; i < 4; ++i)
            if (NkVec2f(x - BtnX(i), y - BtnY()).Len() < W_ * .085f) { Power(i); return; }
        for (int c = 0; c < COLS; ++c) for (int r = 0; r < 3 && r < len_[c]; ++r) {
            if (r > 0 && !hand_) continue;
            if (Hit(x, y, StockX(c), StockY(r), TileSz() * .5f + 4, TileSz() * .5f + 4)) { Deploy(c, r); return; }
        }
    }

    // ---- Rendu -----------------------------------------------------------------
    void DrawBackground(Dl& dl) const {                     // plateau clair derrière l'image
        float x0 = board_->ox - 16, y0 = (board_->oy) * sc_->camera.tilt - 14;
        float w = board_->gw * board_->cs + 32, h = board_->gh * board_->cs * sc_->camera.tilt + board_->ch + 30;
        RoundRectC(dl, x0 + w * .5f, y0 + h * .5f + 3, w, h, 18, NkColor(170, 140, 110, 90));
        RoundRectC(dl, x0 + w * .5f, y0 + h * .5f, w, h, 18, NkColor(240, 228, 214, 255));
        RoundRectC(dl, x0 + w * .5f, y0 + h * .5f, w - 8, h - 8, 14, NkColor(232, 218, 204, 255));
    }

    void DrawUI(Dl& dl) const {
        // --- stock (colonnes) : du fond vers l'avant
        for (int c = 0; c < COLS; ++c) for (int r = NkMin(2, len_[c] - 1); r >= 0; --r) {
            const StockBlock& b = col_[c][r]; bool hidden = b.mystery && r > 0 && !hand_;
            NkString s = hidden ? NkString("?") : NkFormat("{0}", b.count);
            Block3D(dl, StockX(c), StockY(r), TileSz(), board_->palette[b.color], s.CStr(), r > 0 && !hand_, r == 0 || hand_, hidden);
        }
        // --- barre basse + boutons de pouvoir
        RoundRectC(dl, W_ * .5f, H_ * 1.0f, W_ * 1.06f, H_ * .11f, 6, NkColor(150, 60, 205, 255));
        const NkColor bc[4] = {NkColor(110, 200, 90, 255), NkColor(240, 190, 50, 255), NkColor(120, 130, 150, 255), NkColor(70, 150, 230, 255)};
        const char* lab[4] = {"+", "H", "C", "U"};
        for (int i = 0; i < 4; ++i) {
            float x = BtnX(i), y = BtnY(), r = W_ * .075f;
            dl.AddEllipseFilled(NkVec2f(x, y), r + 4, r + 4, NkColor(150, 60, 205, 255)); dl.AddEllipseFilled(NkVec2f(x, y), r, r, NkColor(255, 240, 235, 255));
            dl.AddEllipseFilled(NkVec2f(x, y + 2), r * .78f, r * .78f, bc[i].Shade(.85f)); dl.AddEllipseFilled(NkVec2f(x, y), r * .78f, r * .78f, bc[i]);
            UiText(dl, x, y, r * 1.1f, lab[i], NkColor(255, 255, 255, 255));
            float bx = x + r * .8f, by = y + r * .8f;
            if (pw_[i] > 0) { dl.AddEllipseFilled(NkVec2f(bx, by), r * .36f, r * .36f, NkColor(255, 170, 30, 255)); NkString s = NkFormat("{0}", pw_[i]); UiText(dl, bx, by, r * .5f, s.CStr(), NkColor(255,255,255,255), false); }
            else { dl.AddEllipseFilled(NkVec2f(bx, by), r * .36f, r * .36f, NkColor(70, 200, 70, 255)); UiText(dl, bx, by, r * .6f, "+", NkColor(255,255,255,255), false); }
        }
        // --- slots
        int n = nSlots_; float sp = NkMin(W_ * .16f, W_ * .88f / n), x0 = W_ * .5f - sp * (n - 1) * .5f, ssz = W_ * .125f;
        for (int i = 0; i < n; ++i) {
            float x = x0 + sp * i, y = H_ * .685f;
            if (slot_[i].active) { NkString s = NkFormat("{0}", slot_[i].remaining); Block3D(dl, x, y, ssz, board_->palette[slot_[i].color], s.CStr(), false, false, false); }
            else { RoundRectC(dl, x, y + 5, ssz, ssz, ssz * .2f, NkColor(200, 175, 150, 255)); RoundRectC(dl, x, y, ssz, ssz * .94f, ssz * .2f, NkColor(243, 239, 235, 255)); }
        }
        // --- en-tête
        dl.AddEllipseFilled(NkVec2f(W_ * .075f, H_ * .047f), W_ * .055f, W_ * .055f, NkColor(255, 255, 255, 255));
        dl.AddEllipseFilled(NkVec2f(W_ * .075f, H_ * .047f), W_ * .046f, W_ * .046f, NkColor(150, 60, 205, 255));
        NkString lv = NkFormat("Level {0}", level);
        RoundRectC(dl, W_ * .5f, H_ * .047f, W_ * .27f, H_ * .033f, H_ * .016f, NkColor(150, 60, 205, 255));
        UiText(dl, W_ * .5f, H_ * .047f, H_ * .021f, lv.CStr(), NkColor(255, 255, 255, 255));
        RoundRectC(dl, W_ * .87f, H_ * .047f, W_ * .16f, H_ * .04f, H_ * .02f, NkColor(120, 120, 125, 255));
        NkString sp2 = NkFormat("x{0}", (int)speedMul_); UiText(dl, W_ * .87f, H_ * .047f, H_ * .022f, sp2.CStr(), NkColor(255, 255, 255, 255));
        if (hand_) UiText(dl, W_ * .5f, H_ * .74f - TileSz() * .8f, H_ * .02f, "Choisis un bloc", NkColor(255, 255, 255, 255));
        if (boost_ > 0) UiText(dl, W_ * .5f, H_ * .1f, H_ * .02f, "UFO !", NkColor(255, 240, 120, 255));
        if (state_ != PLAYING) {
            RoundRectC(dl, W_ * .5f, H_ * .4f, W_ * .8f, H_ * .14f, 24, NkColor(40, 20, 60, 215));
            UiText(dl, W_ * .5f, H_ * .385f, H_ * .04f, state_ == WON ? "Niveau termine !" : "Echec", NkColor(255, 255, 255, 255));
        }
    }

private:
    // ---- mise en page ---------------------------------------------------------------
    float TileSz() const { return W_ * .118f; }
    float StockX(int c) const { return W_ * (0.26f + 0.16f * c); }
    float StockY(int r) const { return H_ * (0.765f + 0.07f * r); }
    float BtnX(int i) const { return W_ * (0.176f + 0.217f * i); }
    float BtnY() const { return H_ * .955f; }
    static bool Hit(float x, float y, float cx, float cy, float hw, float hh) { return nkentseu::math::NkFloatRect(cx - hw, cy - hh, 2 * hw, 2 * hh).Contains(x, y); }

    // ---- stock -----------------------------------------------------------------------
    void BuildStock() {
        StockBlock all[MAXALL]; int n = 0; static const int sz[7] = {10, 15, 16, 20, 30, 40, 50};
        for (int c = 0; c < board_->np; ++c) {
            int left = board_->Remaining(c);
            while (left > 0 && n < MAXALL) {
                int cand[7], nc = 0; for (int i = 0; i < 7; ++i) if (sz[i] <= left) cand[nc++] = sz[i];
                int s = nc ? cand[rng_.NextUInt32((uint32)nc)] : left; if (n == MAXALL - 1) s = left;
                all[n++] = { c, s, rng_.NextUInt32(100) < 18 }; left -= s;
            }
        }
        // Mélange de Fisher-Yates : chaque permutation a la même probabilité (n! cas pour n échanges).
        for (int i = n - 1; i > 0; --i) { int j = (int)rng_.NextUInt32((uint32)(i + 1)); StockBlock t = all[i]; all[i] = all[j]; all[j] = t; }
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
                if (NkVec2f(a->x - hx_, a->y - hy_).Len() < 16.f) { Despawn(i); --i; } else if (a->Arrived()) PathHome(a);
                continue;
            }
            if (a->task >= 0) {
                if (!board_->Present(a->task, a->colorIdx)) { a->task = -1; --tasks[a->owner]; }
                else {
                    float qx, qy; board_->ClosestOnCell(a->task, a->x, a->y, qx, qy);
                    if (NkVec2f(qx - a->x, qy - a->y).Len() <= a->Radius() + 3.f) { --tasks[a->owner]; Grab(a); busy = true; continue; }
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
        a->scale = antScale_; a->eatOnArrive = true; a->arriveRadius = 16.f; a->heading = -NK_PI_F * .5f;
        a->speed = 120.f + float(rng_.NextUInt32(40)); a->x = hx_ + float(rng_.NextUInt32(21)) - 10; a->y = hy_ + float(rng_.NextUInt32(11)) - 5;
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
        NkVec2f pts[40]; int n = board_->FindPathToCell(a->x, a->y, a->task, pts, 40); if (n <= 0) return false;
        a->arriveRadius = 5.f; a->SetPath(pts, n, board_->CellX(a->task), board_->CellY(a->task)); return true;
    }
    void PathHome(Ant* a) {                                      // chemin jusqu'au trou
        NkVec2f pts[40]; int n = board_->FindPathToPoint(a->x, a->y, hx_, hy_, pts, 40); a->arriveRadius = 16.f;
        if (n > 0) a->SetPath(pts, n, hx_, hy_); else a->SetTarget(hx_, hy_);
    }

    Scene* sc_ = nullptr; Board* board_ = nullptr; float W_ = 540, H_ = 960, hx_ = 0, hy_ = 0, antScale_ = 1;
    StockBlock col_[COLS][MAXCOL]; int len_[COLS] = {};
    Slot slot_[MAXSLOTS]; int nSlots_ = 5;
    Ant* ants_[MAXANTS]; int na_ = 0;
    int pw_[4] = {1, 0, 0, 1}; bool hand_ = false; float boost_ = 0, speedMul_ = 1;
    int expo_[MAX_PAL] = {}; float expoT_ = 0, stallT_ = 0, autoT_ = 1.0f; bool auto_ = false;
    State state_ = PLAYING; NkLcg32 rng_;
};

} // namespace nk25d
