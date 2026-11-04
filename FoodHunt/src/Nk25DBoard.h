// Nk25DBoard.hpp — Plateau de jeu : image -> blocs de pixels 2.5D, palette, fourmis par couleur.
// Sans STL. L'image arrive en RGBA brut (const unsigned char*, w, h) : à fournir depuis NkImage.
#pragma once
#include "Nk25D.h"

namespace nk25d {

constexpr int MAX_PAL = 16;
constexpr unsigned char EMPTY = 255;

// =============================================================================
//  Board : grille de cubes ; seuls les blocs "exposés" (un voisin vide) sont prenables
// =============================================================================
class Board : public Entity, public ICustomDraw {
public:
    int   gw = 0, gh = 0;                 // taille de la grille
    float cs = 11, ch = 8;                // taille d'une cellule / hauteur d'un cube (px)
    float ox = 0, oy = 0;                 // coin haut-gauche du plateau (monde)
    Col   palette[MAX_PAL]; int np = 0;

    Board() { kind = KIND_BOARD; }
    ~Board() { delete[] c_; delete[] cool_; delete[] acc_; }

    // ---- Image -> grille ------------------------------------------------------
    // gridW : nombre de blocs en largeur ; maxColors : taille de palette (k-means) ;
    // autoBg : retire le fond (couleur des coins, connecté aux bords). Alpha < 128 = vide.
    bool LoadFromRGBA(const unsigned char* px, int w, int h, int gridW, int maxColors, bool autoBg = false) {
        if (!px || w <= 0 || h <= 0 || gridW <= 0) return false;
        delete[] c_; delete[] cool_; delete[] acc_; acc_ = nullptr;
        gw = gridW; gh = m::Max(1, (int)(h * (float)gridW / w + 0.5f));
        int n = gw * gh; c_ = new unsigned char[n]; cool_ = new float[n];
        float* avg = new float[n * 3]; bool* op = new bool[n];
        for (int cy = 0; cy < gh; ++cy) for (int cx = 0; cx < gw; ++cx) {   // moyenne par cellule (pondérée alpha)
            int x0 = cx * w / gw, x1 = m::Max(x0 + 1, (cx + 1) * w / gw), y0 = cy * h / gh, y1 = m::Max(y0 + 1, (cy + 1) * h / gh);
            float r = 0, g = 0, b = 0, aw = 0; int cnt = 0;
            for (int y = y0; y < y1 && y < h; ++y) for (int x = x0; x < x1 && x < w; ++x) {
                const unsigned char* p = px + (y * w + x) * 4; float a = p[3] / 255.f;
                r += p[0] * a; g += p[1] * a; b += p[2] * a; aw += a; ++cnt;
            }
            int i = cy * gw + cx; op[i] = cnt > 0 && aw / cnt >= 0.5f;
            if (aw > 0) { avg[i*3] = r / aw; avg[i*3+1] = g / aw; avg[i*3+2] = b / aw; } else avg[i*3] = avg[i*3+1] = avg[i*3+2] = 0;
        }
        if (autoBg) RemoveBackground(avg, op);
        Quantize(avg, op, m::Clamp(maxColors, 1, (int)MAX_PAL));
        for (int i = 0; i < n; ++i) cool_[i] = 0;
        delete[] avg; delete[] op; time_ = 0; Recount(); UpdateReach();
        return np > 0;
    }

    // ---- Requêtes ---------------------------------------------------------------
    int   Remaining() const { int t = 0; for (int i = 0; i < np; ++i) t += left_[i]; return t; }
    int   Remaining(int color) const { return (color >= 0 && color < np) ? left_[color] : 0; }
    int   At(int cx, int cy) const { return (cx < 0 || cy < 0 || cx >= gw || cy >= gh) ? EMPTY : c_[cy * gw + cx]; }
    float CellX(int i) const { return ox + (i % gw + .5f) * cs; }
    float CellY(int i) const { return oy + (i / gw + .5f) * cs; }
    bool  Exposed(int i) const {
        int cx = i % gw, cy = i / gw;
        return c_[i] != EMPTY && (At(cx-1,cy) == EMPTY || At(cx+1,cy) == EMPTY || At(cx,cy-1) == EMPTY || At(cx,cy+1) == EMPTY);
    }
    // Bloc exposé de la couleur donnée le plus proche de (fx,fy), hors cellules en pause ; -1 si aucun
    int FindNearest(int color, float fx, float fy) const {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < gw * gh; ++i) {
            if (c_[i] != color || cool_[i] > time_ || !Accessible(i)) continue;
            float dx = CellX(i) - fx, dy = CellY(i) - fy, d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }
    // Nombre de blocs exposés par couleur (out[MAX_PAL])
    void  ExposedCounts(int* out) const {
        for (int i = 0; i < MAX_PAL; ++i) out[i] = 0;
        for (int i = 0; i < gw * gh; ++i) if (c_[i] != EMPTY && Accessible(i)) ++out[c_[i]];
    }
    // ---- Accessibilité : un bloc est ciblable si l'une de ses 4 voisines est une cellule vide reliée à l'extérieur
    // (4-connexité à travers les cellules vides). Les poches fermées (yeux retirés...) ne comptent pas.
    bool  Accessible(int i) const { return acc_ ? acc_[i] : Exposed(i); }
    void  UpdateReach() {
        int n = gw * gh; if (!n) return;
        if (!acc_) acc_ = new bool[n];
        bool* reach = new bool[n]; int* st = new int[n + 8]; int sp = 0;
        for (int i = 0; i < n; ++i) reach[i] = false;
        for (int i = 0; i < n; ++i) { int x = i % gw, y = i / gw; if (c_[i] == EMPTY && (x == 0 || y == 0 || x == gw - 1 || y == gh - 1)) { reach[i] = true; st[sp++] = i; } }
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
        while (sp > 0) {
            int i = st[--sp], x = i % gw, y = i / gw;
            for (int k = 0; k < 4; ++k) { int nx = x + dx[k], ny = y + dy[k]; if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                int j = ny * gw + nx; if (c_[j] == EMPTY && !reach[j]) { reach[j] = true; st[sp++] = j; } }
        }
        for (int i = 0; i < n; ++i) {
            acc_[i] = false; if (c_[i] == EMPTY) continue; int x = i % gw, y = i / gw;
            for (int k = 0; k < 4 && !acc_[i]; ++k) { int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) acc_[i] = true; else if (reach[ny * gw + nx]) acc_[i] = true; }
        }
        delete[] reach; delete[] st;
    }
    // ---- Chemin (points de passage monde) jusqu'à une cellule d'accès du bloc `target`, ou jusqu'à un point.
    // BFS sur la grille (zone libre = cellule vide + 4 voisines vides, marge libre autour), puis lissage par ligne de vue.
    int FindPathToCell(float sx, float sy, int target, Vec2* out, int maxN) const { return Search(sx, sy, target, 0, 0, out, maxN); }
    int FindPathToPoint(float sx, float sy, float tx, float ty, Vec2* out, int maxN) const { return Search(sx, sy, -1, tx, ty, out, maxN); }
    bool  Present(int i, int color) const { return i >= 0 && i < gw * gh && c_[i] == color; }
    void  Cooldown(int i, float sec) { if (i >= 0 && i < gw * gh) cool_[i] = time_ + sec; }
    int   Take(int i) { int c = c_[i]; if (c != EMPTY) { c_[i] = EMPTY; --left_[c]; } return c; }   // retire un bloc
    void  ClosestOnCell(int i, float px, float py, float& qx, float& qy) const {
        float x0 = CellX(i) - cs * .5f, y0 = CellY(i) - cs * .5f;
        qx = m::Clamp(px, x0, x0 + cs); qy = m::Clamp(py, y0, y0 + cs);
    }

    // ---- Entity ---------------------------------------------------------------
    void Update(float dt) override { time_ += dt; }

    bool ResolveCircle(float& cx, float& cy, float r, float& nx, float& ny) override {
        r = m::Min(r, cs * 0.42f);                                         // rayon de collision réduit : passe dans les rainures d'1 cellule
        bool hit = false;
        int ix0 = (int)((cx - r - ox) / cs) - 1, ix1 = (int)((cx + r - ox) / cs) + 1;
        int iy0 = (int)((cy - r - oy) / cs) - 1, iy1 = (int)((cy + r - oy) / cs) + 1;
        for (int pass = 0; pass < 2; ++pass)
        for (int y = m::Max(iy0, 0); y <= iy1 && y < gh; ++y) for (int x = m::Max(ix0, 0); x <= ix1 && x < gw; ++x) {
            if (c_[y * gw + x] == EMPTY) continue;
            float qx, qy; ClosestOnCell(y * gw + x, cx, cy, qx, qy);
            float dx = cx - qx, dy = cy - qy, d = m::Hypot(dx, dy);
            if (d >= r) continue;
            if (d > 1e-4f) { nx = dx / d; ny = dy / d; }
            else { float ex = cx - CellX(y * gw + x), ey = cy - CellY(y * gw + x), l = m::Max(1e-4f, m::Hypot(ex, ey)); nx = ex / l; ny = ey / l; d = 0; }
            cx += nx * (r - d); cy += ny * (r - d); hit = true;
        }
        return hit;
    }

    void DrawShadow(IPainter& p, const Camera& cam) const override {   // ombre portée des cubes de bord
        if (!c_) return;
        for (int cy = 0; cy < gh; ++cy) for (int cx = 0; cx < gw; ++cx) {
            if (c_[cy * gw + cx] == EMPTY) continue;
            if (At(cx + 1, cy) != EMPTY && At(cx, cy + 1) != EMPTY) continue;
            float x0 = ox + cx * cs, y0 = oy + cy * cs;
            Vec2 q[4] = { cam.ShadowPos(x0, y0, ch), cam.ShadowPos(x0 + cs, y0, ch), cam.ShadowPos(x0 + cs, y0 + cs, ch), cam.ShadowPos(x0, y0 + cs, ch) };
            p.FillPolygon(q, 4, {0, 0, 0, 45});
        }
    }

    void Emit(RenderQueue& q, const Camera& cam) const override {
        if (!c_) return;
        for (int i = 0; i < gw * gh; ++i)
            if (c_[i] != EMPTY) q.Custom(cam.Depth(0, CellY(i), ch), static_cast<const ICustomDraw*>(this), i);
    }

    // Cube axis-aligned : dessus + face avant (les côtés sont de profil donc invisibles)
    void DrawPiece(IPainter& p, const Camera& cam, int i) const override {
        int cx = i % gw, cy = i / gw; Col col = palette[c_[i]];
        float x0 = ox + cx * cs, y0 = oy + cy * cs, g = 0.3f;   // jeu en x seulement : pas de trait entre rangées
        if (At(cx, cy + 1) == EMPTY) {                                       // face avant visible seulement si rien devant
            Vec2 F[4] = { cam.Project(x0 + g, y0 + cs, 0), cam.Project(x0 + cs - g, y0 + cs, 0),
                          cam.Project(x0 + cs - g, y0 + cs, ch), cam.Project(x0 + g, y0 + cs, ch) };
            p.FillPolygon(F, 4, Shade(col, 0.72f));
        }
        Vec2 T[4] = { cam.Project(x0 + g, y0, ch), cam.Project(x0 + cs - g, y0, ch),
                      cam.Project(x0 + cs - g, y0 + cs, ch), cam.Project(x0 + g, y0 + cs, ch) };
        p.FillPolygon(T, 4, Shade(col, 1.08f));
        float b = cs * 0.2f;                                                 // biseau intérieur plus clair
        Vec2 I[4] = { cam.Project(x0 + b, y0 + b, ch), cam.Project(x0 + cs - b, y0 + b, ch),
                      cam.Project(x0 + cs - b, y0 + cs - b, ch), cam.Project(x0 + b, y0 + cs - b, ch) };
        p.FillPolygon(I, 4, Shade(col, 1.18f));
    }

private:
    int Search(float sx, float sy, int target, float tx, float ty, Vec2* out, int maxN) const {
        const int M = 8, PW = gw + 2 * M, PH = gh + 2 * M, N = PW * PH;
        bool* ok = new bool[N]; bool* goal = new bool[N]; int* par = new int[N]; int* q = new int[N];
        for (int py = 0; py < PH; ++py) for (int px = 0; px < PW; ++px) {
            int x = px - M, y = py - M, k = py * PW + px; goal[k] = false; par[k] = -2;
            ok[k] = (x < 0 || y < 0 || x >= gw || y >= gh) ? true : (c_[y * gw + x] == EMPTY);
        }
        auto cellOf = [&](float wx, float wy, int& px, int& py) {
            px = m::Clamp((int)((wx - ox) / cs + 1000.f) - 1000 + M, 0, PW - 1); py = m::Clamp((int)((wy - oy) / cs + 1000.f) - 1000 + M, 0, PH - 1); };
        if (target >= 0) {
            int x = target % gw, y = target / gw; const int D[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
            for (int k = 0; k < 4; ++k) {
                int gx = x + D[k][0] + M, gy = y + D[k][1] + M;
                if (gx >= 0 && gy >= 0 && gx < PW && gy < PH && ok[gy * PW + gx]) goal[gy * PW + gx] = true;
            }
        } else { int gx, gy; cellOf(tx, ty, gx, gy); ok[gy * PW + gx] = true; goal[gy * PW + gx] = true; }
        int spx, spy; cellOf(sx, sy, spx, spy); int start = spy * PW + spx; ok[start] = true;
        int qh = 0, qt = 0, found = -1; q[qt++] = start; par[start] = -1;
        while (qh < qt) {
            int cur = q[qh++]; if (goal[cur]) { found = cur; break; }
            int cx = cur % PW, cy = cur / PW; const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) { int nx = cx + dx4[k], ny = cy + dy4[k]; if (nx < 0 || ny < 0 || nx >= PW || ny >= PH) continue;
                int j = ny * PW + nx; if (par[j] != -2 || !ok[j]) continue; par[j] = cur; q[qt++] = j; }
        }
        int cnt = 0;
        if (found >= 0) {
            int len = 0, cur = found; while (cur != -1 && len < N) { q[len++] = cur; cur = par[cur]; }      // q = chemin inversé
            // lissage : garder le point le plus lointain en ligne de vue
            auto cx = [&](int k) { return ox + ((k % PW) - M + .5f) * cs; }; auto cy = [&](int k) { return oy + ((k / PW) - M + .5f) * cs; };
            auto los = [&](int a, int bq) { float x0 = cx(a), y0 = cy(a), x1 = cx(bq), y1 = cy(bq), d = m::Hypot(x1 - x0, y1 - y0); int st = (int)(d / (cs * .4f)) + 1;
                for (int i = 1; i < st; ++i) { int px, py; cellOf(x0 + (x1 - x0) * i / st, y0 + (y1 - y0) * i / st, px, py); if (!ok[py * PW + px]) return false; } return true; };
            if (cnt < maxN && len > 1) out[cnt++] = { cx(q[len - 1]), cy(q[len - 1]) };   // d'abord le centre de sa propre cellule (évite de couper un coin)
            int i = len - 1;                                                    // q[len-1] = départ
            while (i > 0 && cnt < maxN) {
                int farv = i - 1; for (int j = 0; j < i; ++j) if (los(q[i], q[j])) { farv = j; break; }
                out[cnt++] = { cx(q[farv]), cy(q[farv]) }; i = farv;
            }
        }
        delete[] ok; delete[] goal; delete[] par; delete[] q;
        return cnt;
    }
    static float D2(const float* a, const float* b) { float x = a[0]-b[0], y = a[1]-b[1], z = a[2]-b[2]; return x*x + y*y + z*z; }

    void RemoveBackground(const float* avg, bool* op) {                       // remplissage depuis les bords
        int n = gw * gh; float bg[3] = {avg[0], avg[1], avg[2]};
        int* st = new int[n]; int sp = 0; bool* seen = new bool[n]; for (int i = 0; i < n; ++i) seen[i] = false;
        for (int x = 0; x < gw; ++x) { st[sp++] = x; st[sp++] = (gh - 1) * gw + x; if (sp + 2 >= n) break; }
        for (int y = 0; y < gh && sp + 2 < n; ++y) { st[sp++] = y * gw; st[sp++] = y * gw + gw - 1; }
        while (sp > 0) {
            int i = st[--sp]; if (seen[i] || !op[i] || D2(avg + i * 3, bg) > 30.f * 30.f) continue;
            seen[i] = true; op[i] = false; int cx = i % gw, cy = i / gw;
            if (sp + 4 >= n * 2) continue;
            if (cx > 0 && sp < n - 4) st[sp++] = i - 1; if (cx < gw - 1 && sp < n - 4) st[sp++] = i + 1;
            if (cy > 0 && sp < n - 4) st[sp++] = i - gw; if (cy < gh - 1 && sp < n - 4) st[sp++] = i + gw;
        }
        delete[] st; delete[] seen;
    }

    void Quantize(const float* avg, const bool* op, int k) {                  // k-means (init : point le plus éloigné)
        int n = gw * gh, cnt = 0; for (int i = 0; i < n; ++i) if (op[i]) ++cnt;
        np = 0; for (int i = 0; i < n; ++i) c_[i] = EMPTY;
        if (!cnt) return;
        k = m::Min(k, cnt);
        float cen[MAX_PAL][3]; int first = 0; while (!op[first]) ++first;
        for (int j = 0; j < 3; ++j) cen[0][j] = avg[first * 3 + j];
        for (int c = 1; c < k; ++c) {
            float bd = -1; int bi = first;
            for (int i = 0; i < n; ++i) { if (!op[i]) continue;
                float md = 1e30f; for (int q = 0; q < c; ++q) md = m::Min(md, D2(avg + i * 3, cen[q]));
                if (md > bd) { bd = md; bi = i; } }
            for (int j = 0; j < 3; ++j) cen[c][j] = avg[bi * 3 + j];
        }
        for (int it = 0; it < 8; ++it) {
            float sum[MAX_PAL][3] = {}; int num[MAX_PAL] = {};
            for (int i = 0; i < n; ++i) { if (!op[i]) continue;
                int b = 0; float bd = 1e30f; for (int q = 0; q < k; ++q) { float d = D2(avg + i * 3, cen[q]); if (d < bd) { bd = d; b = q; } }
                for (int j = 0; j < 3; ++j) sum[b][j] += avg[i * 3 + j]; ++num[b]; }
            for (int q = 0; q < k; ++q) if (num[q]) for (int j = 0; j < 3; ++j) cen[q][j] = sum[q][j] / num[q];
        }
        int remap[MAX_PAL]; for (int q = 0; q < MAX_PAL; ++q) remap[q] = -1;
        for (int i = 0; i < n; ++i) { if (!op[i]) continue;
            int b = 0; float bd = 1e30f; for (int q = 0; q < k; ++q) { float d = D2(avg + i * 3, cen[q]); if (d < bd) { bd = d; b = q; } }
            if (remap[b] < 0) { remap[b] = np; palette[np++] = { (unsigned char)(cen[b][0] + .5f), (unsigned char)(cen[b][1] + .5f), (unsigned char)(cen[b][2] + .5f), 255 }; }
            c_[i] = (unsigned char)remap[b]; }
    }
    void Recount() { for (int i = 0; i < MAX_PAL; ++i) left_[i] = 0; for (int i = 0; i < gw * gh; ++i) if (c_[i] != EMPTY) ++left_[c_[i]]; }

    unsigned char* c_ = nullptr; float* cool_ = nullptr; bool* acc_ = nullptr; int left_[MAX_PAL] = {}; float time_ = 0;
};

// =============================================================================
//  Hole : trou de nid (décor au sol). Les fourmis y apportent les blocs.
// =============================================================================
class Hole : public Entity {
public:
    float rx = 22, ry = 14;
    Hole() { kind = KIND_HOLE; }
    void Update(float) override {}
    void DrawShadow(IPainter& p, const Camera& cam) const override {
        Vec2 c = cam.Project(x, y, 0);
        p.FillEllipse({c.x, c.y + 1.5f}, rx + 3, ry + 3, {120, 80, 50, 255});
        p.FillEllipse(c, rx, ry, {55, 35, 25, 255});
        p.FillEllipse({c.x, c.y - 2}, rx * .7f, ry * .6f, {30, 18, 12, 255});
    }
    void Emit(RenderQueue&, const Camera&) const override {}
};

// =============================================================================
//  PixelGame : une fourmi ne prend QUE les blocs de SA couleur, les apporte au nid.
// =============================================================================
class PixelGame {
public:
    enum { MAX_ANTS = 48 };
    void Init(Scene* sc, Board* b, float nestX, float nestY) {
        sc_ = sc; board_ = b; nx_ = nestX; ny_ = nestY; na_ = 0;
        Hole* h = sc->Add<Hole>(); h->x = nestX; h->y = nestY;
    }
    // Ajoute une fourmi de la couleur de palette `colorIdx`
    Ant* AddAnt(int colorIdx, float scale = 1.0f) {
        if (na_ >= MAX_ANTS || colorIdx < 0 || colorIdx >= board_->np) return nullptr;
        Ant* a = sc_->Add<Ant>(); a->colorIdx = colorIdx; a->color = board_->palette[colorIdx];
        a->scale = scale; a->eatOnArrive = true; a->arriveRadius = 16.f;   // entre dans le trou sans file d'attente
        a->x = nx_ + float((na_ * 37) % 31 - 15); a->y = ny_ + float((na_ * 53) % 21 - 10);
        a->speed = 130.f + float((na_ * 13) % 40); ants_[na_++] = a; return a;
    }
    // Répartit `total` fourmis proportionnellement au nombre de blocs de chaque couleur (min. 1 par couleur)
    void SpawnByShare(int total, float scale = 1.0f) {
        int all = board_->Remaining(); if (all <= 0) return;
        for (int c = 0; c < board_->np; ++c) {
            int n = m::Max(1, (int)((float)total * board_->Remaining(c) / all + 0.5f));
            for (int k = 0; k < n; ++k) AddAnt(c, scale);
        }
    }
    int  AntCount() const { return na_; }
    Ant* AntAt(int i) const { return ants_[i]; }
    bool Done() const { return board_->Remaining() == 0; }

    void Update(float dt) {
        for (int i = 0; i < na_; ++i) {
            Ant* a = ants_[i];
            if (a->Busy()) continue;                                         // en transport
            if (a->task >= 0) {
                if (!board_->Present(a->task, a->colorIdx)) { a->task = -1; }
                else {
                    float qx, qy; board_->ClosestOnCell(a->task, a->x, a->y, qx, qy);
                    if (m::Hypot(qx - a->x, qy - a->y) <= a->Radius() + 3.f) { Grab(a); continue; }
                    if (a->Stuck() >= 3) { board_->Cooldown(a->task, 10.f); a->task = -1; a->ResetStuck(); }   // inaccessible : on change
                    else a->SetTarget(board_->CellX(a->task), board_->CellY(a->task));
                    if (a->task >= 0) continue;
                }
            }
            int c = board_->FindNearest(a->colorIdx, a->x, a->y);
            if (c >= 0) { a->task = c; a->ResetStuck(); a->SetTarget(board_->CellX(c), board_->CellY(c)); board_->Cooldown(c, 0.f); }
            else if (a->Arrived()) a->SetTarget(nx_ + float((i * 71) % 120 - 60), ny_ + float((i * 29) % 60 - 20));   // rien à prendre : attend vers le nid
        }
        (void)dt;
    }
private:
    void Grab(Ant* a) {
        int cell = a->task; float cx = board_->CellX(cell), cy = board_->CellY(cell);
        int col = board_->Take(cell);
        Box* b = sc_->Add<Box>(); b->w = b->d = board_->cs; b->h = board_->ch; b->color = board_->palette[col];
        b->showText = false; b->carryScale = 0.8f; b->x = cx; b->y = cy;
        a->task = -1; a->PickUp(b, nx_, ny_);
    }
    Scene* sc_ = nullptr; Board* board_ = nullptr; Ant* ants_[MAX_ANTS]; int na_ = 0; float nx_ = 0, ny_ = 0;
};

} // namespace nk25d