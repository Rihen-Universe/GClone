// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DGame.h — Gameplay « tiroirs » : stock de blocs numérotés (rayon de miel) -> 5 paniers -> fourmis -> image.
// Migré le 04/11/2026 sur Nkentseu, réécrit le 05/11/2026 : habillage « pique-nique » (Nk25DArt.h), règle des
// fourmis de Rodolf (un seul voyage, compte exact, sortie sous condition), mise en page pour tout écran.
//
// COORDONNÉES. Le jeu raisonne en coordonnées LOGIQUES de largeur >= 540 et de hauteur >= 1000 : le plan de la vue
// NkView25D (NkView25D.h), dont l'échelle s = min(W / 540, H / 1000) porte l'adaptation à l'écran (SetWindow). Le MONDE (cubes,
// fourmis, nid) ne bouge jamais quand l'écran change : c'est la VUE (NkView25D::center) qui se déplace. Ainsi un
// redimensionnement ne déplace aucune fourmi et ne recalcule aucun chemin.
//
// LA RÈGLE DES FOURMIS (Rodolf, 05/11/2026) :
//   1. une fourmi naît au TIROIR (le panier), jamais au nid ;
//   2. elle fait UN SEUL voyage : tiroir -> son bloc réservé -> nid, puis elle disparaît et ne ressort plus ;
//   3. un bloc numéroté N fait sortir exactement N fourmis ; le nombre du panier baisse à chaque sortie ;
//   4. une fourmi ne sort que si, à cet instant, on lui RÉSERVE un bloc de sa couleur, accessible (relié à l'extérieur
//      par des cases vides, jamais encadré), non réservé, avec un chemin trouvé. Sinon elle attend dans le tiroir ;
//   5. le tiroir est libéré dès que sa dernière fourmi est sortie.
// Le jeu COMPTE tout cela (struct Stats) : le banc `--verifier-niveaux` s'y appuie.
#pragma once
#include "Nk25DBoard.h"
#include "Nk25DArt.h"

namespace nk25d {

// ---- Primitives UI (espace logique) -----------------------------------------------
// Texte centré avec ombre : le texte de l'interface passe par NkTexteCentreTaille (police embarquée de NKFont).
inline void UiText(Dl& dl, float x, float y, float size, const char* t, const NkColor& c, bool shadow = true) {
    GameText(dl, x, y, size, t, c, shadow);
}

// Rectangle arrondi centré en (cx, cy) : le rayon est borné à la moitié du petit côté (au-delà, ce serait un disque).
inline void RoundRectC(Dl& dl, float cx, float cy, float w, float h, float r, const NkColor& c) {
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h), c, NkMin(r, NkMin(w, h) * .499f));
}

// Bloc "bonbon" 3D de l'interface (ANCIEN, 04/11/2026 ; remplacé par l'alvéole et le panier, gardé et non appelé).
// Faux relief fait de TROIS rectangles arrondis superposés : lèvre sombre (teinte x 0.66, décalée de 7 % vers le bas),
// face avant, reflet plus clair (x 1.13). Le texte est sombre sur fond clair, blanc sur fond foncé, selon la luminance
// lum = (3 R + 6 G + B) / 10 (approximation entière de la Rec. 601).
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

// Une alvéole du stock. Les champs d'animation (yoff... flipT) ne servent qu'à l'affichage : `yoff` est le décalage VERTICAL
// (pixels logiques, positif = plus bas) entre la place de l'alvéole dans sa colonne et l'endroit où elle est dessinée ; il
// revient à 0 en ressort amorti (glissement vers le haut avec petit rebond), après `delay` secondes (décalage entre alvéoles).
struct StockBlock {
    int color; int count; bool mystery;
    float yoff = 0, vy = 0, delay = 0, appear = 1.f;       // glissement, vitesse, attente, apparition 0..1
    bool revealed = false; float flipT = -1.f;             // mystère : révélé ? bascule 0..1 (-1 : pas commencée)
};
// Un tiroir. `flying` : le bloc vole encore jusqu'au panier ; il ne fait pas sortir de fourmis avant d'être arrivé.
struct Slot { bool active = false, flying = false; int color = -1, remaining = 0; float spawnCd = 0; };
// Vol d'une alvéole cliquée jusqu'à son panier : de (x0, y0) à la place du panier, 0.25 s, départ vif et arrivée douce.
struct Fly { bool on = false; float t = 0, x0 = 0, y0 = 0; int color = 0, count = 0; bool mystery = false; };

// Les COMPTEURS du jeu : ce que le banc vérifie (voir Verify dans main.cpp).
struct Stats {
    int expected = 0;      // somme des numéros du stock = nombre de fourmis attendues
    int spawned = 0;       // fourmis sorties d'un tiroir
    int returned = 0;      // fourmis revenues au nid avec un bloc
    int unreserved = 0;    // sorties SANS bloc réservé (doit rester 0)
    int dupExit = 0;       // fourmis ressorties après un premier voyage (doit rester 0)
    int tapped = 0;        // blocs posés par un CLIC (OnTap) : le jeu n'a pas d'autre chemin pour poser un bloc
    int blocks = 0;        // nombre de blocs du stock
    int dupReserve = 0;    // blocs réservés deux fois (doit rester 0)
    int lostBlock = 0;     // bloc réservé disparu avant le ramassage (doit rester 0)
    bool balanced = true;  // somme des numéros du stock == nombre de cubes, pour CHAQUE couleur
};

class Gameplay {
public:
    enum State { PLAYING, WON, LOST };
    enum Action { ACT_NONE, ACT_NEXT, ACT_RESTART };
    enum { MAXSLOTS = 7, COLS = 4, MAXCOL = 64, MAXANTS = 120, MAXALL = 256, MAXCRUMB = 96, MAXCONF = 120 };
    int level = 1;                                           // numéro AFFICHÉ (1..25)
    Stats stats;

    // ---- Monde : posé UNE FOIS par niveau --------------------------------------
    // Les cubes occupent x dans [0, gw cs], y dans [0, gh cs] ; le nid est à 5.4 cases sous le plateau. La taille d'une
    // case cs ne dépend que de la largeur de l'image et d'un plafond de hauteur FIXE (430 px d'écran) : elle ne change
    // donc jamais avec l'écran (voir l'en-tête).
    void Init(Scene* sc, Board* b, int levelNumber) {
        sc_ = sc; board_ = b; level = levelNumber; na_ = 0; state_ = PLAYING; endT_ = 0; stallT_ = 0; tPlay_ = 0;
        sc->camera.tilt = 0.85f;
        const float tilt = sc->camera.tilt;
        float cs = (float)(int)(540.f * 0.86f / b->gw), csH = (float)(int)(430.f / (b->gh * tilt + 0.8f));
        b->cs = NkMax(4.f, NkMin(cs, csH)); b->ch = b->cs * 0.45f; b->ox = 0.f; b->oy = 0.f;
        hx_ = b->gw * b->cs * .5f; hy_ = b->gh * b->cs + 5.4f * b->cs;
        Hole* h = sc->Add<Hole>(); h->x = hx_; h->y = hy_; h->rx = b->cs * 3.4f; h->ry = h->rx * 0.65f; holeRy_ = h->ry;
        antScale_ = b->cs / 17.f; nSlots_ = 5; rng_.Seed(1234567u + (uint32)levelNumber * 7919u);
        for (int i = 0; i < MAXSLOTS; ++i) { slot_[i] = Slot(); bz_[i] = bv_[i] = 0; slotT_[i] = 1.f; }
        pw_[0] = 1; pw_[1] = 0; pw_[2] = 0; pw_[3] = 1; hand_ = false; boost_ = 0; bonusUsed_ = 0; ncr_ = 0; ncf_ = 0;
        stats = Stats();
        for (int i = 0; i < MAXSLOTS; ++i) fly_[i] = Fly();
        resv_.Resize((usize)(b->gw * b->gh)); for (usize i = 0; i < resv_.Size(); ++i) resv_[i] = 0;
        total_ = NkMax(1, b->Remaining());
        BuildStock();
        stats.blocks = 0; for (int c = 0; c < COLS; ++c) stats.blocks += len_[c];
        SetWindow(winW_, winH_);
    }

    // ---- Vue : se recalcule à chaque changement de taille de fenêtre ---------------------
    // La vue (NkView25D) porte l'adaptation à l'écran : Adapt règle `size` (zone visible, >= 540 x 1000 unités) et `window`
    // (pixels), puis on place le groupe « assiette + nid » au milieu de la zone libre entre l'en-tête et les paniers en
    // décalant `center`. Le MONDE ne bouge jamais : seule la vue change (un redimensionnement ne déplace aucune fourmi).
    // Les coordonnées LOGIQUES de l'interface sont celles du plan de la vue : [0, size.x] x [0, size.y].
    //
    // ZONE SÛRE (05/11/2026). Sur téléphone, une encoche, la barre de statut ou l'indicateur de geste couvrent les bords de
    // l'écran. L'herbe et la nappe y vont quand même (DrawBackground, sinon une bande vide), mais rien de ce qu'on lit ou touche.
    // La vue s'adapte donc au rectangle utile (fenêtre moins les marges), et tout le jeu est décalé de (safeL_, safeT_) pixels.
    void SetSafeArea(float top, float bottom, float left, float right) { safeT_ = top; safeB_ = bottom; safeL_ = left; safeR_ = right; }
    float SafeLeft() const { return safeL_; }
    float SafeTop() const { return safeT_; }
    void SetWindow(float w, float h) {
        winW_ = w; winH_ = h;
        if (!sc_) return;
        NkView25D& cam = sc_->camera;
        cam.Adapt(NkMax(1.f, w - safeL_ - safeR_), NkMax(1.f, h - safeT_ - safeB_), 540.f, 1000.f);
        LW_ = cam.size.x; LH_ = cam.size.y;
        if (!board_) return;
        const float tilt = cam.tilt, cs = board_->cs, ch = board_->ch;
        const float availTop = 92.f, availBot = LH_ - 398.f;
        const float groupH = ch + 22.f + hy_ * tilt + holeRy_ * 1.4f;
        float top = availTop + NkMax(0.f, (availBot - availTop - groupH) * .5f);
        const NkVec2f origin(LW_ * .5f - board_->gw * cs * .5f, top + ch + 22.f);   // où tombe le point (0, 0) du monde, en logique
        cam.center = cam.size * .5f - origin;                                          // le coin haut-gauche visible est -origin
        cam.center.x -= safeL_ / cam.Scale(); cam.center.y -= safeT_ / cam.Scale();    // Project rend des pixels : décalage de la zone sûre
        plL_ = origin.x - 20.f; plR_ = origin.x + board_->gw * cs + 20.f; plT_ = origin.y - ch - 22.f; plB_ = origin.y + board_->gh * cs * tilt + 18.f;
    }
    float ViewScale() const { return sc_ ? sc_->camera.Scale() : 1.f; }       // pixels par unité logique

    void AutoPlay(bool on) { auto_ = on; }
    // Le BOT JOUE PAR LE CLIC (05/11) : il ne pose jamais un bloc lui-même ; il rend le point (logique) où il « appuie », et
    // l'appelant l'envoie par le chemin du vrai clic (pixels d'écran -> OnTap). Rend faux s'il n'a rien à faire à cet instant.
    bool BotTick(float dt, float& lx, float& ly) {
        if ((autoT_ -= dt) > 0) return false;
        autoT_ = botPeriod_; return BotChoose(lx, ly);
    }
    void SetBotPeriod(float s) { botPeriod_ = s; }
    void SetLegacy(bool on) { legacy_ = on; }                // ANCIEN comportement des fourmis (contre-épreuve du banc)
    void BreakLevel() { nSlots_ = 0; }                       // contre-épreuve : un niveau INJOUABLE (aucun tiroir)
    State GetState() const { return state_; }
    float TimeScale() const { return speedMul_; }
    float PlayTime() const { return tPlay_; }
    float EndTime() const { return endT_; }
    // Centre (logique) de l'alvéole (c, r) là où elle est DESSINÉE (animation comprise) : pour les essais de clic.
    bool CellPoint(int c, int r, float& lx, float& ly) const { if (c < 0 || c >= COLS || r >= len_[c]) return false; HexPos(c, r, lx, ly); ly += col_[c][r].yoff; return true; }
    // Diagnostic du banc (défaite) : tiroirs, cubes restants et exposés par couleur, blocs de stock restants par couleur.
    NkString DescribeState() const {
        NkString r; int ex[MAX_PAL]; board_->ExposedCounts(ex);
        for (int s = 0; s < nSlots_; ++s) r += NkFormat(" T{0}:{1}", s, slot_[s].active ? slot_[s].color : -1) + (slot_[s].active ? NkFormat("x{0}", slot_[s].remaining) : NkString());
        for (int c = 0; c < board_->np; ++c) {
            int st = 0; for (int k = 0; k < COLS; ++k) for (int q = 0; q < len_[k]; ++q) if (col_[k][q].color == c) st += col_[k][q].count;
            r += NkFormat(" c{0}[reste {1} expose {2} stock {3}]", c, board_->Remaining(c), ex[c], st);
        }
        return r;
    }
    // Diagnostic du banc : l'état des premières fourmis encore dehors (position en cases, bloc visé, chargée ?).
    NkString DescribeAnts() const {
        NkString r;
        for (int i = 0; i < na_ && i < 4; ++i) {
            const Ant* a = ants_[i]; int cx = (int)(a->x / board_->cs), cy = (int)(a->y / board_->cs); NkVec2f pp[400]; int np2 = board_->FindPathToPoint(a->x, a->y, hx_, hy_, pp, 400), nq = board_->FindPathToPoint(a->x, a->y, hx_, hy_, pp, 40);
            r += NkFormat(" [fourmi case({0},{1}) sur {6} grille {7}x{8} cible {2} chargee {3} bloquee {4} arrivee {5} chemin {9}/{10} pos({11},{12}) nid({13},{14}) cap {15} vit {16} fant {17}]", cx, cy, a->task, (int)a->Busy(), a->Stuck(), (int)a->Arrived(), board_->At(cx, cy), board_->gw, board_->gh, np2, nq, (int)a->x, (int)a->y, (int)hx_, (int)hy_, (int)(a->heading * 100.f), (int)a->speed, (int)(a->ghost * 10.f));
        }
        for (int i = 0; i < na_ && i < 1; ++i) {            // la première fourmi : ses points de passage et la carte 7x7 autour d'elle
            const Ant* a = ants_[i]; int cx = (int)(a->x / board_->cs), cy = (int)(a->y / board_->cs);
            r += NkFormat(" wp {0}/{1}:", a->WpIndex(), a->WpCount());
            for (int k = a->WpIndex(); k < a->WpCount() && k < a->WpIndex() + 4; ++k) r += NkFormat(" ({0},{1})", (int)a->Wp(k).x, (int)a->Wp(k).y);
            r += NkFormat(" cible({0},{1}) carte:", (int)a->Target().x, (int)a->Target().y);
            for (int dy = -3; dy <= 3; ++dy) { r += " "; for (int dx = -3; dx <= 3; ++dx) r += board_->At(cx + dx, cy + dy) == EMPTY ? "." : "#"; }
        }
        return r;
    }
    int   Ants() const { return na_; }
    int   StockLeft() const { int t = 0; for (int c = 0; c < COLS; ++c) t += len_[c]; return t; }

    // Avance le jeu (sous-pas <= 1/30 s pour rester stable à vitesse x3)
    void Tick(float dt) {
        float d = dt * speedMul_;
        while (d > 1e-5f) { float s = NkMin(d, 1.f / 30.f); Step(s); d -= s; }
    }

    // ---- Entrées (coordonnées LOGIQUES) ------------------------------------------
    Action OnTap(float x, float y) {
        if (state_ != PLAYING) {
            if (endT_ > 0.5f && Hit(x, y, LW_ * .5f, EndBtnY(), 135.f, 33.f)) return state_ == WON ? ACT_NEXT : ACT_RESTART;
            return ACT_NONE;
        }
        if (Hit(x, y, LW_ - 70.f, 40.f, 46.f, 26.f)) { speedMul_ = speedMul_ >= 3.f ? 1.f : speedMul_ + 1.f; return ACT_NONE; }
        for (int i = 0; i < 4; ++i) if (Hit(x, y, BtnX(i), BtnY(), 36.f, 36.f)) { Power(i); return ACT_NONE; }
        for (int c = 0; c < COLS; ++c) for (int r = 0; r < 3 && r < len_[c]; ++r) {
            if (r > 0 && !hand_) continue;
            // Un clic est accepté PENDANT les animations : on teste la place de l'alvéole ET l'endroit où elle est dessinée.
            float cx, cy; HexPos(c, r, cx, cy);
            if (InHexFlat(x, y, cx, cy, kHexR) || InHexFlat(x, y, cx, cy + col_[c][r].yoff, kHexR)) { if (Deploy(c, r)) ++stats.tapped; return ACT_NONE; }
        }
        return ACT_NONE;
    }

    // ---- Rendu -----------------------------------------------------------------
    // Zone visible entière, en logique : [0, LW] x [0, LH].
    void DrawBackground(Dl& dl) const {
        // Le décor déborde sous les marges de la zone sûre : en coordonnées logiques, l'écran entier commence avant 0.
        const float k = ViewScale(), x0 = -safeL_ / k, y0 = -safeT_ / k, x1 = LW_ + safeR_ / k, y1 = LH_ + safeB_ / k;
        DrawGrass(dl, x0, y0, x1, y1);
        DrawPlate(dl, plL_, plT_, plR_, plB_);
        DrawGingham(dl, x0, LH_ - 96.f, x1, y1);
    }

    // Miettes et confettis : au-dessus de la scène, sous l'interface.
    void DrawEffects(Dl& dl) const {
        for (int i = 0; i < ncr_; ++i) {
            const Crumb& c = crumb_[i]; float a = NkClamp(c.life / 0.25f, 0.f, 1.f);
            dl.AddEllipseFilled(NkVec2f(c.x, c.y), 2.4f, 1.1f, NkColor(30, 20, 10, (uint8)(60 * a)));                // ombre au sol
            dl.AddEllipseFilled(NkVec2f(c.x, c.y - c.z), 3.4f, 3.0f, c.c.Shade(0.7f));
            dl.AddEllipseFilled(NkVec2f(c.x - .5f, c.y - c.z - .5f), 2.6f, 2.3f, NkColor(c.c.r, c.c.g, c.c.b, (uint8)(255 * a)));
        }
        for (int i = 0; i < ncf_; ++i) {
            const Confetti& f = conf_[i]; if (f.y < -20.f) continue;
            DrawConfetti(dl, f.x, f.y, f.rot, f.w * (0.35f + 0.65f * NkAbs(NkCos(f.rot * 1.7f))), f.c);
        }
    }

    void DrawUI(Dl& dl) const {
        // --- en-tête : niveau, progression, vitesse
        NkString lv = NkFormat("Niveau {0}", level);
        Pill(dl, LW_ * .5f, 40.f, 210.f, 46.f, pal::Tomate());
        GameTextOutline(dl, LW_ * .5f, 39.f, 26.f, lv.CStr(), NkColor(255, 255, 255, 255), NkColor(120, 36, 28, 255));
        float prog = NkClamp(1.f - (float)board_->Remaining() / (float)total_, 0.f, 1.f);
        RoundRectC(dl, LW_ * .5f, 78.f, 230.f, 14.f, 7.f, NkColor(98, 62, 42, 150));
        if (prog > 0.01f) dl.AddRectFilled(nkgui::NkRect(LW_ * .5f - 113.f, 72.f, 226.f * prog, 12.f), pal::Feuille(), 6.f);
        NkString sp2 = NkFormat("x{0}", (int)speedMul_);
        Pill(dl, LW_ - 70.f, 40.f, 84.f, 40.f, pal::Ciel());
        GameTextOutline(dl, LW_ - 70.f, 39.f, 24.f, sp2.CStr(), NkColor(255, 255, 255, 255), NkColor(30, 70, 120, 255));

        // --- stock : le rayon de miel (colonnes droites). D'abord les creux, puis les alvéoles du fond vers l'avant.
        for (int c = 0; c < COLS; ++c) for (int r = 2; r >= 0; --r) { float cx, cy; HexPos(c, r, cx, cy); if (r >= len_[c]) DrawEmptyCell(dl, cx, cy, kHexR); }
        for (int c = 0; c < COLS; ++c) for (int r = NkMin(2, len_[c] - 1); r >= 0; --r) {
            const StockBlock& b = col_[c][r]; bool dispo = r == 0 || hand_, shown = !b.mystery || b.revealed || hand_;
            float sx = 1.f; if (b.flipT >= 0.f && b.flipT < 1.f) { sx = NkAbs(NkCos(NK_PI_F * b.flipT)); if (sx < 0.06f) sx = 0.06f; }
            NkString s = shown ? NkFormat("{0}", b.count) : NkString("?");
            float cx, cy; HexPos(c, r, cx, cy);
            float e = b.appear; float k = 0.45f + 0.55f * (1.f - (1.f - e) * (1.f - e));                    // apparition : la cellule grandit
            DrawHoneyCell(dl, cx, cy + b.yoff, kHexR, board_->palette[b.color], s.CStr(), dispo, !shown, dispo, k, sx);
        }
        if (hand_) { Pill(dl, LW_ * .5f, LH_ - 316.f + 6.f, 196.f, 30.f, pal::Moutarde()); GameTextOutline(dl, LW_ * .5f, LH_ - 316.f + 5.f, 20.f, "Choisis un bloc", NkColor(255, 255, 255, 255), pal::Brun()); }

        // --- paniers (les tiroirs)
        for (int i = 0; i < nSlots_; ++i) {
            float x, y, sz; BasketPos(i, x, y, sz);
            if (slot_[i].active && !slot_[i].flying) {
                float t = NkClamp(slotT_[i] / 0.3f, 0.f, 1.f), k = 1.f + 2.7f * (t - 1.f) * (t - 1.f) * (t - 1.f) + 1.7f * (t - 1.f) * (t - 1.f);   // sortie avec dépassement (easeOutBack)
                NkString s = NkFormat("{0}", slot_[i].remaining);
                DrawBasket(dl, x, y, sz * NkMax(0.05f, k), board_->palette[slot_[i].color], s.CStr(), true, bz_[i]);
            } else DrawBasket(dl, x, y, sz, NkColor(255, 255, 255, 255), "", false, 0.f);
        }

        // --- alvéoles en vol : de leur place jusqu'au panier. Départ vif et arrivée douce (sortie cubique : e = 1 - (1 - t)^3),
        //     léger arc (on remonte de sin(pi t) * 44 px au milieu du trajet), la cellule rétrécit un peu en arrivant.
        for (int i = 0; i < nSlots_; ++i) if (fly_[i].on) {
            float bx, by, bsz; BasketPos(i, bx, by, bsz); const Fly& f = fly_[i];
            float t = NkClamp(f.t, 0.f, 1.f), e = 1.f - (1.f - t) * (1.f - t) * (1.f - t);
            float x = f.x0 + (bx - f.x0) * e, y = f.y0 + (by - f.y0) * e - NkSin(NK_PI_F * t) * 44.f;
            NkString s = f.mystery ? NkFormat("{0}", f.count) : NkFormat("{0}", f.count);
            DrawHoneyCell(dl, x, y, kHexR, board_->palette[f.color], s.CStr(), true, false, true, 1.f - 0.30f * e);
        }

        // --- boutons de bonus : quatre carrés d'osier, une icône dessinée chacun
        const NkColor bc[4] = {pal::Feuille(), pal::Moutarde(), pal::Tomate(), pal::Ciel()};
        for (int i = 0; i < 4; ++i) {
            float x = BtnX(i), y = BtnY(), r = 31.f;
            Pill(dl, x, y, r * 2.f, r * 2.f, bc[i], r * .5f);
            if (i == 0) IconPanier(dl, x, y - 2.f, r * .74f); else if (i == 1) IconMain(dl, x, y + 1.f, r * .62f);
            else if (i == 2) IconBombe(dl, x, y + 1.f, r * .66f); else IconEclair(dl, x, y - 1.f, r * .66f);
            float bx = x + r * .78f, by = y - r * .78f;
            if (pw_[i] > 0) { dl.AddEllipseFilled(NkVec2f(bx, by), 11.f, 11.f, NkColor(255, 255, 255, 255)); dl.AddEllipseFilled(NkVec2f(bx, by), 9.f, 9.f, NkColor(255, 150, 30, 255)); NkString s = NkFormat("{0}", pw_[i]); UiText(dl, bx, by, 14.f, s.CStr(), NkColor(255, 255, 255, 255), false); }
            else { dl.AddEllipseFilled(NkVec2f(bx, by), 11.f, 11.f, NkColor(255, 255, 255, 255)); dl.AddEllipseFilled(NkVec2f(bx, by), 9.f, 9.f, NkColor(70, 190, 70, 255)); UiText(dl, bx, by, 16.f, "+", NkColor(255, 255, 255, 255), false); }
        }
        if (boost_ > 0) { Pill(dl, LW_ * .5f, 100.f, 120.f, 28.f, pal::Moutarde()); GameTextOutline(dl, LW_ * .5f, 99.f, 19.f, "Éclair !", NkColor(255, 255, 255, 255), pal::Brun()); }

        if (state_ != PLAYING) DrawEnd(dl);
    }

private:
    // ---- mise en page (logique) ---------------------------------------------------------------
    static constexpr float kHexR = 36.f;                                    // rayon des alvéoles (sommet-centre)
    // Alvéole (colonne c, rang r) : hexagones à SOMMETS PLATS, nid d'abeilles par COLONNES. Colonnes espacées de 1.5 R ;
    // rangs espacés de sqrt(3) R DANS la colonne (donc strictement l'un sous l'autre : une alvéole qui monte monte tout droit) ;
    // une colonne sur deux décalée d'une demi-hauteur (sqrt(3) R / 2), ce qui fait le quinconce du rayon de miel.
    static float RowStep() { return 1.7320508f * kHexR; }
    void HexPos(int c, int r, float& x, float& y) const {
        x = LW_ * .5f + (c - 1.5f) * 1.6f * kHexR; y = LH_ - 268.f + RowStep() * r + ((c & 1) ? RowStep() * .5f : 0.f);
    }
    float BtnX(int i) const { return LW_ * .5f + (i - 1.5f) * 118.f; }
    float BtnY() const { return LH_ - 46.f; }
    float EndBtnY() const { return LH_ * .5f - 30.f + 118.f; }
    void BasketPos(int i, float& x, float& y, float& sz) const {
        int n = nSlots_ < 1 ? 1 : nSlots_; float sp = NkMin(88.f, 520.f / n);
        x = LW_ * .5f - sp * (n - 1) * .5f + sp * i; y = LH_ - 348.f; sz = NkMin(78.f, sp * .9f);
    }
    static bool Hit(float x, float y, float cx, float cy, float hw, float hh) { return NkAbs(x - cx) <= hw && NkAbs(y - cy) <= hh; }
    // Point de l'écran (logique) -> point du monde (sol, z = 0) : inverse de NkView25D::Project (Unproject).
    NkVec2f ToWorld(float sx, float sy) const { const float k = sc_->camera.Scale(); return sc_->camera.Unproject(sx * k, sy * k); }

    // ---- stock -----------------------------------------------------------------------
    // Le numéro d'un bloc de stock est le nombre de fourmis qu'il fait sortir. Pour que le jeu soit gagnable, la somme
    // des numéros d'une couleur doit valoir EXACTEMENT le nombre de cubes de cette couleur (règle 5) : on découpe
    // chaque couleur en morceaux de taille tirée dans {10,15,16,20,30,40,50}, le dernier morceau prenant le reste.
    // STOCK GAGNABLE PAR CONSTRUCTION (05/11/2026). Un mélange au hasard (BuildStockAleatoire, plus bas) produit des
    // niveaux perdus d'avance : cinq tiroirs remplis de couleurs enfouies, pendant que les blocs qui les découvriraient
    // dorment au fond des colonnes (mesuré par le banc : niveaux 20 et 24). On construit donc le stock en SIMULANT
    // l'épluchage du plateau : à chaque tour, on regarde les cubes ACCESSIBLES (reliés à l'extérieur par des cases vides),
    // on prend la couleur qui en offre le plus, on en retire un morceau de taille N tirée dans {10,15,16,20,30,40,50}
    // (au plus le nombre de cubes accessibles de cette couleur ; s'il y en a moins de 10, tous), et ce morceau devient un
    // bloc de stock numéroté N. La suite obtenue est un ordre de jeu valide : chaque bloc est entièrement satisfaisable
    // dès que les précédents sont vidés. Les blocs sont ensuite répartis en quatre colonnes À LA SUITE (bloc k dans la
    // colonne k mod 4) : le plus ancien bloc non déployé est toujours en tête d'une colonne, donc jouable. La somme des
    // numéros d'une couleur vaut exactement le nombre de ses cubes (règle 5), puisque chaque cube est retiré une fois.
    void BuildStock() {
        const int gw = board_->gw, gh = board_->gh, n = gw * gh;
        NkVector<uint8> g; g.Resize((usize)n); for (int i = 0; i < n; ++i) g[i] = (uint8)board_->At(i % gw, i / gw);
        NkVector<uint8> reach; reach.Resize((usize)n); NkVector<int> st;
        StockBlock all[MAXALL]; int nb = 0, left = board_->Remaining(); static const int sz[7] = {10, 15, 16, 20, 30, 40, 50};
        int sum[MAX_PAL] = {};
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
        while (left > 0 && nb < MAXALL) {
            for (int i = 0; i < n; ++i) reach[i] = 0;                                     // 1. cases vides reliées au bord (remplissage)
            st.Clear();
            for (int i = 0; i < n; ++i) { int x = i % gw, y = i / gw; if (g[i] == EMPTY && (x == 0 || y == 0 || x == gw - 1 || y == gh - 1)) { reach[i] = 1; st.PushBack(i); } }
            while (st.Size() > 0) {
                int i = st[st.Size() - 1]; st.PopBack(); int x = i % gw, y = i / gw;
                for (int k = 0; k < 4; ++k) { int nx = x + dx[k], ny = y + dy[k]; if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    int j = ny * gw + nx; if (g[j] == EMPTY && !reach[j]) { reach[j] = 1; st.PushBack(j); } }
            }
            int cnt[MAX_PAL] = {};                                                        // 2. cubes accessibles par couleur
            for (int i = 0; i < n; ++i) if (g[i] != EMPTY) { int x = i % gw, y = i / gw; bool acc = false;
                for (int k = 0; k < 4 && !acc; ++k) { int nx = x + dx[k], ny = y + dy[k]; acc = nx < 0 || ny < 0 || nx >= gw || ny >= gh || reach[ny * gw + nx]; }
                if (acc) ++cnt[g[i]]; }
            int bc = -1; for (int c = 0; c < board_->np; ++c) if (cnt[c] > 0 && (bc < 0 || cnt[c] > cnt[bc])) bc = c;
            if (bc < 0) break;
            int cand[7], nc = 0; for (int i = 0; i < 7; ++i) if (sz[i] <= cnt[bc]) cand[nc++] = sz[i];   // 3. taille du morceau
            int take = nc ? cand[rng_.NextUInt32((uint32)nc)] : cnt[bc];
            for (int i = 0, got = 0; i < n && got < take; ++i) if (g[i] == bc) { int x = i % gw, y = i / gw; bool acc = false;   // 4. on les retire
                for (int k = 0; k < 4 && !acc; ++k) { int nx = x + dx[k], ny = y + dy[k]; acc = nx < 0 || ny < 0 || nx >= gw || ny >= gh || reach[ny * gw + nx]; }
                if (acc) { g[i] = EMPTY; ++got; } }
            all[nb++] = { bc, take, rng_.NextUInt32(100) < 18 }; left -= take; sum[bc] += take;
        }
        stats.expected = 0; stats.balanced = left == 0;
        for (int c = 0; c < board_->np; ++c) { stats.expected += sum[c]; if (sum[c] != board_->Remaining(c)) stats.balanced = false; }
        for (int c = 0; c < COLS; ++c) len_[c] = 0;
        for (int i = 0; i < nb; ++i) { int c = i % COLS; if (len_[c] < MAXCOL) col_[c][len_[c]++] = all[i]; }   // à la suite, PAS mélangés
    }

    // Stock ALÉATOIRE (04/11/2026), gardé et non appelé : « on ne supprime pas ». Raison du retrait : des niveaux perdus d'avance (voir ci-dessus).
    void BuildStockAleatoire() {
        StockBlock all[MAXALL]; int n = 0; static const int sz[7] = {10, 15, 16, 20, 30, 40, 50};
        int sum[MAX_PAL] = {};
        for (int c = 0; c < board_->np; ++c) {
            int left = board_->Remaining(c);
            while (left > 0 && n < MAXALL) {
                int cand[7], nc = 0; for (int i = 0; i < 7; ++i) if (sz[i] <= left) cand[nc++] = sz[i];
                int s = nc ? cand[rng_.NextUInt32((uint32)nc)] : left; if (n == MAXALL - 1) s = left;
                all[n++] = { c, s, rng_.NextUInt32(100) < 18 }; left -= s; sum[c] += s;
            }
        }
        stats.expected = 0; stats.balanced = true;
        for (int c = 0; c < board_->np; ++c) { stats.expected += sum[c]; if (sum[c] != board_->Remaining(c)) stats.balanced = false; }
        // Mélange de Fisher-Yates : chaque permutation a la même probabilité (n! cas pour n échanges).
        for (int i = n - 1; i > 0; --i) { int j = (int)rng_.NextUInt32((uint32)(i + 1)); StockBlock t = all[i]; all[i] = all[j]; all[j] = t; }
        for (int c = 0; c < COLS; ++c) len_[c] = 0;
        for (int i = 0; i < n; ++i) { int c = i % COLS; if (len_[c] < MAXCOL) col_[c][len_[c]++] = all[i]; }
    }
    bool FreeSlot(int& out) const { for (int i = 0; i < nSlots_; ++i) if (!slot_[i].active) { out = i; return true; } return false; }
    // Pose le bloc (c, r) dans un tiroir libre. RIEN ne se téléporte : l'alvéole VOLE jusqu'au panier (Fly), le panier ne fait
    // sortir ses fourmis qu'à l'arrivée, et les alvéoles suivantes de la colonne GLISSENT vers le haut l'une après l'autre
    // (décalage de 30 ms, rebond d'arrivée). Rend faux s'il n'y a pas de tiroir libre.
    bool Deploy(int c, int r) {
        int si; if (!FreeSlot(si)) return false;
        StockBlock b = col_[c][r]; float fx, fy; HexPos(c, r, fx, fy); fy += b.yoff;
        for (int k = r; k + 1 < len_[c]; ++k) col_[c][k] = col_[c][k + 1];
        --len_[c];
        for (int k = r; k < len_[c]; ++k) {                                   // les suivantes montent d'un rang : elles partent de l'ancienne place (plus bas)
            StockBlock& n = col_[c][k]; n.yoff += RowStep(); n.vy = 0; n.delay = 0.03f * (float)(k - r);
            if (k == 2) n.appear = 0.f;                                      // celle qui entre dans les trois rangs visibles apparaît en fondu
        }
        slot_[si].active = true; slot_[si].flying = true; slot_[si].color = b.color; slot_[si].remaining = b.count; slot_[si].spawnCd = 0.1f;
        fly_[si].on = true; fly_[si].t = 0; fly_[si].x0 = fx; fly_[si].y0 = fy; fly_[si].color = b.color; fly_[si].count = b.count; fly_[si].mystery = false;
        hand_ = false;
        return true;
    }
    void Power(int i) {
        if (pw_[i] <= 0) { pw_[i] = 1; return; }                 // bouton "+" : place de la pub / récompense
        if (i == 0) { if (nSlots_ >= MAXSLOTS) return; --pw_[0]; ++nSlots_; slotT_[nSlots_ - 1] = 0; }
        else if (i == 1) { --pw_[1]; hand_ = true; }
        else if (i == 2) { --pw_[2]; Cannon(); }
        else { --pw_[3]; boost_ = 15.f; }
        ++bonusUsed_;
    }
    void Cannon() {                                              // retire 15 blocs exposés de la couleur la plus exposée
        int ex[MAX_PAL]; board_->ExposedCounts(ex); int bc = 0;
        for (int c = 1; c < board_->np; ++c) if (ex[c] > ex[bc]) bc = c;
        for (int k = 0; k < 15; ++k) { int cell = board_->FindNearest(bc, hx_, hy_); if (cell < 0) break; board_->Take(cell); }
    }

    // ---- boucle de jeu ---------------------------------------------------------------
    void Step(float dt) {
        sc_->Update(dt);
        UpdateFx(dt);
        if (state_ != PLAYING) { endT_ += dt; return; }
        tPlay_ += dt;
        if (boost_ > 0) boost_ -= dt;

        // 1. SORTIES : chaque tiroir fait sortir une fourmi tous les 0.22 s (0.09 s en mode éclair), mais SEULEMENT si un bloc
        //    peut lui être réservé à cet instant (règle 4). La lecture de l'accessibilité est fraîche (Board::EnsureReach).
        for (int s = 0; s < nSlots_; ++s) {
            Slot& sl = slot_[s]; if (!sl.active || sl.flying) continue;
            if (sl.remaining <= 0) { sl.active = false; continue; }
            sl.spawnCd -= dt;
            if (sl.spawnCd <= 0 && na_ < MAXANTS) TrySpawn(s);
        }

        // 2. FOURMIS : un seul voyage, aucune ne repart chercher un second bloc (règle 2).
        for (int i = 0; i < na_; ++i) {
            Ant* a = ants_[i];
            if (a->justAte) {
                a->justAte = false; ++stats.returned; OnEaten(a);
                if (a->trips > 1) ++stats.dupExit;
                if (!legacy_) { Despawn(i); --i; continue; }
                a->task = -1;                                    // ANCIEN comportement (contre-épreuve) : elle repart chercher un autre bloc
                int cell = board_->FindNearest(a->colorIdx, a->x, a->y);
                if (cell >= 0) { Reserve(cell); a->task = cell; PathToCell(a); } else a->task = -2;
            }
            if (a->Busy()) { if (a->Stuck() >= 2) { a->ghost = 2.f; Unjam(a); PathHome(a); a->ResetStuck(); } continue; }       // chargée et coincée : on recalcule le chemin
            if (a->task >= 0) {
                if (!board_->Present(a->task, a->colorIdx)) { ++stats.lostBlock; a->task = -2; PathHome(a); continue; }
                float qx, qy; board_->ClosestOnCell(a->task, a->x, a->y, qx, qy);
                if (NkVec2f(qx - a->x, qy - a->y).Len() <= a->Radius() + 3.f) { Grab(a); continue; }
                // Embouteillage : elle recalcule son chemin vers LE MÊME bloc, elle ne l'abandonne pas et ne rentre pas à vide.
                if (a->Stuck() >= 2 || a->Arrived()) { if (a->Stuck() >= 2) { a->ghost = 2.f; Unjam(a); } a->ResetStuck(); PathToCell(a); }
                continue;
            }
            if (NkVec2f(a->x - hx_, a->y - hy_).Len() < 16.f) { Despawn(i); --i; } else if (a->Arrived()) PathHome(a);   // ancien comportement seulement
        }

        // 3. VICTOIRE / DÉFAITE
        if (board_->Remaining() == 0 && na_ == 0) { Win(); return; }
        bool progress = na_ > 0;
        if (!progress) {
            int ex[MAX_PAL]; board_->ExposedCounts(ex);
            for (int s = 0; s < nSlots_ && !progress; ++s) if (slot_[s].active && slot_[s].remaining > 0 && ex[slot_[s].color] > 0) progress = true;
        }
        int fs; bool freeSlot = FreeSlot(fs) && StockLeft() > 0;
        if (!progress && !freeSlot) { if ((stallT_ += dt) > 1.0f) { state_ = LOST; endT_ = 0; } } else stallT_ = 0;
    }

    void Reserve(int cell) {
        if (cell < 0 || cell >= (int)resv_.Size()) return;
        if (resv_[cell]) ++stats.dupReserve;
        resv_[cell] = 1; board_->Cooldown(cell, 1e6f);          // réservation ferme : plus personne ne peut viser ce bloc
    }
    // Tente de faire sortir UNE fourmi du tiroir s : on cherche un bloc de sa couleur, accessible et libre, puis un chemin.
    // Un bloc sans chemin est mis de côté 3 s et on en essaie un autre (4 essais). Rien de trouvé : elle attend dans le tiroir.
    void TrySpawn(int s) {
        Slot& sl = slot_[s]; float sx, sy, sz; BasketPos(s, sx, sy, sz);
        NkVec2f w = ToWorld(sx, sy - 6.f), pts[160]; int n = 0, cell = -1;
        if (legacy_) { w = NkVec2f(hx_, hy_); }                  // ANCIEN : naissance au nid, sans réservation
        else for (int t = 0; t < 4; ++t) {
            cell = board_->FindNearest(sl.color, w.x, w.y); if (cell < 0) break;
            n = board_->FindPathToCell(w.x, w.y, cell, pts, 160); if (n > 0) break;
            board_->Cooldown(cell, 3.f); cell = -1;
        }
        if (!legacy_ && cell < 0) { sl.spawnCd = 0.12f; return; }
        if (cell >= 0) Reserve(cell);
        SpawnAnt(s, w.x, w.y, cell, pts, n);
        --sl.remaining; sl.spawnCd = boost_ > 0 ? 0.09f : 0.22f;
        if (sl.remaining <= 0) sl.active = false;                // règle 5 : tiroir libéré dès sa dernière sortie
    }
    void SpawnAnt(int s, float wx, float wy, int cell, const NkVec2f* pts, int n) {
        Ant* a = sc_->Add<Ant>(); a->colorIdx = slot_[s].color; a->owner = s; a->color = board_->palette[a->colorIdx];
        a->scale = antScale_; a->eatOnArrive = true; a->arriveRadius = 5.f;
        a->speed = 120.f + float(rng_.NextUInt32(40)); a->x = wx + float(rng_.NextUInt32(9)) - 4; a->y = wy + float(rng_.NextUInt32(5)) - 2;
        a->heading = -NK_PI_F * .5f; a->task = cell;
        if (cell >= 0 && n > 0) a->SetPath(pts, n, board_->CellX(cell), board_->CellY(cell));
        else if (cell >= 0) a->SetTarget(board_->CellX(cell), board_->CellY(cell));
        else ++stats.unreserved;
        ants_[na_++] = a; ++stats.spawned;
    }
    void Despawn(int i) { Ant* a = ants_[i]; a->dead = true; ants_[i] = ants_[--na_]; }
    void Grab(Ant* a) {
        int cell = a->task; float cx = board_->CellX(cell), cy = board_->CellY(cell);
        int col = board_->Take(cell); if (cell < (int)resv_.Size()) resv_[cell] = 0;
        Box* b = sc_->Add<Box>(); b->w = b->d = board_->cs; b->h = board_->ch; b->color = board_->palette[col];
        b->showText = false; b->carryScale = 0.8f; b->x = cx; b->y = cy;
        a->task = -1; a->PickUp(b, hx_, hy_); PathHome(a);
    }
    // DÉBLOCAGE. Une fourmi qui frotte un coin de cube (le chemin lissé rase les arêtes, son rayon de collision est de
    // 0.42 case) ou qui a été poussée dans un cube par la foule peut rester coincée indéfiniment : le banc en a mesuré
    // (niveaux de plus de 1100 cubes). Au troisième blocage, on la recale au centre de la case libre la plus proche
    // (jamais plus d'une case ou deux de déplacement), puis son chemin est recalculé. Les deux premiers blocages
    // ne font que recalculer le chemin et rendre la fourmi traversable par les autres (fantôme).
    void Unjam(Ant* a) {
        if (++a->jams < 3) return;
        a->jams = 0;
        const float cs = board_->cs; int cx = (int)NkFloor(a->x / cs), cy = (int)NkFloor(a->y / cs), bx = cx, by = cy; float bd = 1e30f;
        for (int r = 0; r <= 3; ++r) {
            for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
                if (NkAbs(dx) != r && NkAbs(dy) != r) continue;
                if (board_->At(cx + dx, cy + dy) != EMPTY) continue;
                float px = (cx + dx + .5f) * cs - a->x, py = (cy + dy + .5f) * cs - a->y, d = px * px + py * py;
                if (d < bd) { bd = d; bx = cx + dx; by = cy + dy; }
            }
            if (bd < 1e29f) break;
        }
        a->x = (bx + .5f) * cs; a->y = (by + .5f) * cs;
    }
    bool PathToCell(Ant* a) {                                    // chemin jusqu'au bloc réservé
        NkVec2f pts[160]; int n = board_->FindPathToCell(a->x, a->y, a->task, pts, 160); if (n <= 0) return false;
        a->arriveRadius = 5.f; a->SetPath(pts, n, board_->CellX(a->task), board_->CellY(a->task)); return true;
    }
    void PathHome(Ant* a) {                                      // chemin jusqu'au trou
        NkVec2f pts[160]; int n = board_->FindPathToPoint(a->x, a->y, hx_, hy_, pts, 160); a->arriveRadius = 16.f;
        if (n > 0) a->SetPath(pts, n, hx_, hy_); else a->SetTarget(hx_, hy_);
    }

    // ---- bot : joue seul (--bot) ---------------------------------------------------------
    // Couleur accessible d'abord, petit bloc d'abord. Si rien n'est accessible et que rien n'avance, il utilise la main
    // (bonus) pour piocher plus profond, ou à défaut débloque n'importe quel bloc.
    bool BotChoose(float& lx, float& ly) {
        int fs; if (!FreeSlot(fs)) return false;
        int ex[MAX_PAL]; board_->ExposedCounts(ex);
        // `need[c]` : fourmis encore attendues par les tiroirs DÉJÀ actifs de la couleur c (elles se partageront les cubes exposés).
        int need[MAX_PAL] = {}; for (int s = 0; s < nSlots_; ++s) if (slot_[s].active) need[slot_[s].color] += slot_[s].remaining;
        // Un bloc est « faisable » quand les cubes exposés non encore promis suffisent à vider tout son tiroir : il ne bloquera
        // pas de tiroir à attendre qu'un autre bloc s'ouvre (l'impasse typique : cinq tiroirs qui attendent des couleurs que
        // seuls les blocs du stock, qu'on ne peut plus poser, découvriraient). Les faisables passent d'abord, le plus petit
        // d'abord (il se libère vite et découvre de nouveaux cubes) ; sinon le plus de cubes disponibles.
        int bestc = -1, bestr = 0, bs = -2000000000;
        for (int c = 0; c < COLS; ++c) for (int r = 0; r < (hand_ ? NkMin(3, len_[c]) : NkMin(1, len_[c])); ++r) {
            int col = col_[c][r].color, avail = ex[col] - need[col]; if (avail <= 0) continue;
            int sc = avail >= col_[c][r].count ? 1000000 - col_[c][r].count * 100 - r : avail * 10 - col_[c][r].count - r;
            if (sc > bs) { bs = sc; bestc = c; bestr = r; }
        }
        // Pas de bloc faisable : on ne pose un bloc partiel que s'il reste au moins DEUX tiroirs libres (le dernier tiroir
        // libre est gardé pour un bloc faisable) ; sinon on attend, ou on pioche plus profond avec la main (bonus).
        if (bestc >= 0 && bs < 1000000) {
            int libres = 0; for (int s = 0; s < nSlots_; ++s) if (!slot_[s].active) ++libres;
            bool prog = na_ > 0; for (int s = 0; s < nSlots_; ++s) if (slot_[s].active && slot_[s].remaining > 0 && ex[slot_[s].color] > 0) prog = true;
            if (libres < 2 && prog) { if (!hand_ && pw_[1] > 0) { lx = BtnX(1); ly = BtnY(); return true; } return false; }
        }
        if (bestc < 0) {
            bool prog = na_ > 0; for (int s = 0; s < nSlots_; ++s) if (slot_[s].active && slot_[s].remaining > 0 && ex[slot_[s].color] > 0) prog = true;
            if (prog) return false;
            if (!hand_ && pw_[1] > 0) { lx = BtnX(1); ly = BtnY(); return true; }
            for (int c = 0; c < COLS; ++c) if (len_[c] > 0) { bestc = c; bestr = 0; break; }
        }
        if (bestc < 0) return false;
        float cx, cy; HexPos(bestc, bestr, cx, cy); lx = cx; ly = cy + col_[bestc][bestr].yoff; return true;          // le point d'appui : l'alvéole choisie
    }

    // ---- vie : miettes, rebond, confettis, écran de fin --------------------------------------
    struct Crumb { float x, y, z, vx, vz, life; NkColor c; };
    struct Confetti { float x, y, vx, vy, rot, vr, w, life; NkColor c; };

    void OnEaten(Ant* a) {                                       // un bloc est mangé : le panier rebondit, des miettes sautent du nid
        if (a->owner >= 0 && a->owner < MAXSLOTS) bv_[a->owner] += 5.5f;
        NkVec2f h = sc_->camera.Project(hx_, hy_, 0.f) / sc_->camera.Scale();      // pixels -> logique
        for (int k = 0; k < 7 && ncr_ < MAXCRUMB; ++k) {
            Crumb& c = crumb_[ncr_++]; c.x = h.x + float(rng_.NextUInt32(15)) - 7.f; c.y = h.y + 2.f; c.z = 4.f;
            c.vx = (float)((int)rng_.NextUInt32(100) - 50); c.vz = 110.f + (float)rng_.NextUInt32(90); c.life = 0.55f + (float)rng_.NextUInt32(25) * 0.01f;
            c.c = a->ateColor.Shade(0.85f + (float)rng_.NextUInt32(30) * 0.01f);
        }
    }
    void UpdateFx(float dt) {
        for (int i = 0; i < ncr_; ++i) {                         // gravité sur la hauteur z, rebond amorti au sol
            Crumb& c = crumb_[i]; c.life -= dt; c.vz -= 520.f * dt; c.z += c.vz * dt; c.x += c.vx * dt;
            if (c.z < 0) { c.z = 0; c.vz = -c.vz * 0.35f; c.vx *= 0.6f; }
            if (c.life <= 0) { crumb_[i--] = crumb_[--ncr_]; }
        }
        for (int i = 0; i < ncf_; ++i) {
            Confetti& f = conf_[i]; f.life -= dt; f.x += f.vx * dt + NkSin(f.rot) * 12.f * dt; f.y += f.vy * dt; f.rot += f.vr * dt; f.vy += 40.f * dt;
            if (f.life <= 0 || f.y > LH_ + 30.f) { conf_[i--] = conf_[--ncf_]; }
        }
        for (int c = 0; c < COLS; ++c) for (int k = 0; k < len_[c] && k < 3; ++k) {     // glissement des alvéoles vers le haut
            StockBlock& b = col_[c][k];
            if (b.appear < 1.f) { b.appear += dt / 0.25f; if (b.appear > 1.f) b.appear = 1.f; }
            if (b.delay > 0.f) { b.delay -= dt; continue; }
            // Ressort amorti : a = -K y - C v, avec K = 300 et C = 24, soit un taux d'amortissement C / (2 sqrt K) = 0.69 :
            // un dépassement d'environ 5 % (le « petit rebond d'arrivée ») et une stabilisation en ~0.3 s.
            b.vy += (-300.f * b.yoff - 24.f * b.vy) * dt; b.yoff += b.vy * dt;
            if (NkAbs(b.yoff) < 0.05f && NkAbs(b.vy) < 0.5f) { b.yoff = 0.f; b.vy = 0.f; }
            if (k == 0 && b.mystery && !b.revealed && b.flipT < 0.f && NkAbs(b.yoff) < 8.f) b.flipT = 0.f;   // le mystère de tête se retourne
            if (b.flipT >= 0.f && b.flipT < 1.f) { b.flipT += dt / 0.35f; if (b.flipT >= 0.5f) b.revealed = true; if (b.flipT > 1.f) b.flipT = 1.f; }
        }
        for (int i = 0; i < MAXSLOTS; ++i) if (fly_[i].on) {      // vols : 0.25 s ; à l'arrivée le panier s'ouvre et ses fourmis peuvent sortir
            fly_[i].t += dt / 0.25f;
            if (fly_[i].t >= 1.f) { fly_[i].on = false; slot_[i].flying = false; slot_[i].spawnCd = 0.05f; slotT_[i] = 0; bv_[i] += 4.f; }
        }
        for (int i = 0; i < MAXSLOTS; ++i) {                     // ressort amorti du rebond : a = -k z - c v
            bv_[i] += (-140.f * bz_[i] - 9.f * bv_[i]) * dt; bz_[i] += bv_[i] * dt; slotT_[i] += dt;
        }
    }
    void Win() {
        state_ = WON; endT_ = 0; stars_ = bonusUsed_ == 0 ? 3 : (bonusUsed_ <= 2 ? 2 : 1);
        static const NkColor pc[6] = {NkColor(226, 76, 60, 255), NkColor(244, 190, 62, 255), NkColor(98, 170, 72, 255), NkColor(98, 162, 222, 255), NkColor(240, 120, 180, 255), NkColor(255, 255, 255, 255)};
        for (ncf_ = 0; ncf_ < MAXCONF; ++ncf_) {
            Confetti& f = conf_[ncf_]; f.x = (float)rng_.NextUInt32((uint32)LW_); f.y = -(float)rng_.NextUInt32(400);
            f.vx = (float)((int)rng_.NextUInt32(80) - 40); f.vy = 90.f + (float)rng_.NextUInt32(150); f.rot = (float)rng_.NextUInt32(628) * .01f;
            f.vr = (float)((int)rng_.NextUInt32(12) - 6); f.w = 10.f + (float)rng_.NextUInt32(8); f.life = 7.f; f.c = pc[rng_.NextUInt32(6)];
        }
    }
    // Écran de fin : voile sombre, carte crème qui grandit (easeOutBack), étoiles, bouton. Le même dessin sert la victoire
    // (étoiles + « Niveau suivant ») et l'échec (« Recommencer »).
    void DrawEnd(Dl& dl) const {
        const bool win = state_ == WON;
        { const float k = ViewScale();                                          // le voile couvre aussi les marges de la zone sûre
          dl.AddRectFilled(nkgui::NkRect(-safeL_ / k, -safeT_ / k, LW_ + (safeL_ + safeR_) / k, LH_ + (safeT_ + safeB_) / k), NkColor(20, 40, 15, (uint8)(NkMin(1.f, endT_ / 0.3f) * 120))); }
        float t = NkClamp(endT_ / 0.45f, 0.f, 1.f), u = t - 1.f, k = 1.f + 2.7f * u * u * u + 1.7f * u * u;       // easeOutBack
        const float cx = LW_ * .5f, cy = LH_ * .5f - 30.f;
        Card(dl, cx, cy, 380.f * k, 330.f * k, pal::Creme(), 36.f * k);
        dl.AddRect(nkgui::NkRect(cx - 190.f * k + 9.f, cy - 165.f * k + 9.f, 362.f * k, 306.f * k), win ? pal::Moutarde() : pal::Tomate(), 3.f, 28.f * k);
        if (t < 0.6f) return;                                                                                         // le contenu attend que la carte soit ouverte
        GameTextOutline(dl, cx, cy - 108.f, 40.f, win ? "Niveau réussi !" : "Échec", win ? pal::Feuille() : pal::Tomate(), NkColor(255, 255, 255, 255));
        if (win) for (int i = 0; i < 3; ++i) {
            float p = NkClamp((endT_ - 0.5f - 0.22f * i) / 0.3f, 0.f, 1.f), v = p - 1.f, kk = p <= 0 ? 0.f : 1.f + 2.7f * v * v * v + 1.7f * v * v;
            float sx = cx + (i - 1) * 84.f, sy = cy - 30.f - (i == 1 ? 12.f : 0.f), R = 36.f * kk;
            bool on = i < stars_;
            Star(dl, sx, sy + 3.f, R * 1.1f, NkColor(120, 80, 20, on ? 120 : 40));
            Star(dl, sx, sy, R * 1.08f, on ? NkColor(214, 140, 20, 255) : NkColor(170, 160, 140, 255));
            Star(dl, sx, sy, R * .9f, on ? pal::Moutarde() : NkColor(205, 198, 182, 255));
        } else {
            GameTextOutline(dl, cx, cy - 36.f, 22.f, "Les fourmis sont bloquées.", pal::Brun(), NkColor(255, 247, 228, 255));
            GameTextOutline(dl, cx, cy - 6.f, 22.f, "Essaie un autre ordre !", pal::Brun(), NkColor(255, 247, 228, 255));
        }
        float p = NkClamp((endT_ - 0.9f) / 0.25f, 0.f, 1.f);
        if (endT_ > 0.5f) { Pill(dl, cx, EndBtnY(), 270.f * (0.7f + 0.3f * p), 66.f, win ? pal::Feuille() : pal::Tomate());
                            GameTextOutline(dl, cx, EndBtnY() - 2.f, 28.f, win ? "Niveau suivant" : "Recommencer", NkColor(255, 255, 255, 255), win ? NkColor(40, 90, 30, 255) : NkColor(120, 36, 28, 255)); }
    }

    Scene* sc_ = nullptr; Board* board_ = nullptr; float LW_ = 540, LH_ = 1000, hx_ = 0, hy_ = 0, holeRy_ = 20, antScale_ = 1;
    float safeT_ = 0, safeB_ = 0, safeL_ = 0, safeR_ = 0;                    // marges de la zone sûre, en pixels
    float winW_ = 540, winH_ = 1000;
    float plL_ = 0, plR_ = 0, plT_ = 0, plB_ = 0;
    StockBlock col_[COLS][MAXCOL]; int len_[COLS] = {};
    Fly fly_[MAXSLOTS];
    Slot slot_[MAXSLOTS]; int nSlots_ = 5; float bz_[MAXSLOTS] = {}, bv_[MAXSLOTS] = {}, slotT_[MAXSLOTS] = {};
    Ant* ants_[MAXANTS]; int na_ = 0;
    Crumb crumb_[MAXCRUMB]; int ncr_ = 0; Confetti conf_[MAXCONF]; int ncf_ = 0;
    NkVector<uint8> resv_;
    int pw_[4] = {1, 0, 0, 1}; bool hand_ = false; float boost_ = 0, speedMul_ = 1;
    int total_ = 1, bonusUsed_ = 0, stars_ = 3; float tPlay_ = 0, endT_ = 0, stallT_ = 0, autoT_ = 1.0f, botPeriod_ = 0.9f; bool auto_ = false, legacy_ = false;
    State state_ = PLAYING; NkLcg32 rng_;
};

} // namespace nk25d
