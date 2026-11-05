// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DBoard.h — Plateau de jeu : image -> blocs de pixels 2.5D, palette, fourmis par couleur.
// Migré le 04/11/2026 : l'image arrive en NkImage (NKImage), la réduction par NK_AREA, la palette par k-means
// (NkImagePalette::Quantize), les chemins par NkGridPath (NKNavigation). Aucune bibliothèque standard.
#pragma once
#include "Nk25D.h"
#include "Nk25DArt.h"
#include "NKImage/Core/NkImage.h"
#include "NKImage/Core/NkImagePalette.h"
#include "NKNavigation/NkGridPath.h"

namespace nk25d {

constexpr int MAX_PAL = 16;
constexpr unsigned char EMPTY = 255;

// =============================================================================
//  Board : grille de cubes ; seuls les blocs "exposés" (un voisin vide) sont prenables
// =============================================================================
// Chaque case contient l'indice de sa couleur dans la palette, ou EMPTY. La grille est un graphe : les cases
// vides sont les noeuds libres, reliés à leurs 4 voisines. Un bloc est « accessible » si une de ses voisines est
// une case vide reliée au bord (remplissage par diffusion depuis l'extérieur) : les poches fermées au milieu du
// dessin (un oeil retiré, par exemple) ne comptent pas, car aucune fourmi ne peut y entrer.
class Board : public Entity, public ICustomDraw {
public:
    int   gw = 0, gh = 0;                 // taille de la grille
    float cs = 11, ch = 8;                // taille d'une cellule / hauteur d'un cube (px)
    float ox = 0, oy = 0;                 // coin haut-gauche du plateau (monde)
    NkColor palette[MAX_PAL]; int np = 0;

    Board() { kind = KIND_BOARD; }

    // ---- Image -> grille ------------------------------------------------------
    // gridW : nombre de blocs en largeur ; maxColors : taille de palette (k-means) ;
    // autoBg : retire le fond (couleur du pourtour, relié aux bords). Alpha < 128 = vide.
    // 1. Réduction : NkImage::Resize(NK_AREA) fait la moyenne de chaque cellule pondérée par l'alpha.
    // 2. Fond (facultatif) : NkImagePalette::EstimateBg puis RemoveBackground.
    // 3. Quantification : k-means sur les couleurs des cellules opaques (NkImagePalette::Quantize).
    bool LoadFromImage(const NkImage& src, int gridW, int maxColors, bool autoBg = false) {
        if (!src.IsValid() || gridW <= 0) return false;
        gw = gridW; gh = NkMax(1, (int)(src.Height() * (float)gridW / src.Width() + 0.5f));
        NkImage rgba = (src.Format() == NkImagePixelFormat::NK_RGBA32) ? src.Copy() : src.Convert(NkImagePixelFormat::NK_RGBA32);
        if (!rgba.IsValid()) return false;
        NkImage cells = rgba.Resize(gw, gh, NkResizeFilter::NK_AREA);
        if (!cells.IsValid()) return false;
        if (autoBg) NkImagePalette::RemoveBackground(cells, NkImagePalette::EstimateBg(cells), 30);

        // PALETTE DE JEU (05/11/2026). Avant : k-means (NkImagePalette::Quantize) donnait des teintes quelconques, parfois deux
        // couleurs presque identiques, jamais les mêmes d'un écran à l'autre. Maintenant chaque cellule opaque est ramenée à la
        // plus proche des dix couleurs FIXES et bien séparées de pal::GameColor ; on ne garde que les `maxColors` plus
        // fréquentes (les autres cellules passent à la plus proche des gardées) ; les doublons sont fusionnés d'office puisque
        // deux cellules ramenées à la même couleur de jeu ont le même indice. L'image reste reconnaissable, aucun niveau n'a
        // deux couleurs proches. Le compte par couleur (left_) se refait ensuite (Recount) : il suit la NOUVELLE palette.
        const int n = gw * gh;
        c_.Resize(n); cool_.Resize(n); acc_.Clear();
        for (int i = 0; i < n; ++i) { c_[i] = EMPTY; cool_[i] = 0; }
        np = 0;
        NkVector<uint8> g; g.Resize((usize)n); int cnt[kNbGameColors] = {};
        for (int i = 0; i < n; ++i) {
            const uint8* px = cells.RowPtr(i / gw) + (i % gw) * 4;
            if (px[3] < 128) { g[i] = EMPTY; continue; }                                 // case vide
            g[i] = (uint8)NearestGameColor(px[0], px[1], px[2]); ++cnt[g[i]];
        }
        unsigned keep = 0; int order[kNbGameColors], nk = 0;
        for (int k = 0; k < NkClamp(maxColors, 1, (int)kNbGameColors); ++k) {
            int b = -1; for (int c = 0; c < kNbGameColors; ++c) if (!(keep & (1u << c)) && cnt[c] > 0 && (b < 0 || cnt[c] > cnt[b])) b = c;
            if (b < 0) break; keep |= 1u << b; order[nk++] = b;
        }
        int remap[kNbGameColors]; for (int c = 0; c < kNbGameColors; ++c) remap[c] = -1;
        for (int k = 0; k < nk; ++k) { remap[order[k]] = np; palette[np++] = GameColor(order[k]); }
        for (int c = 0; c < kNbGameColors; ++c) if (remap[c] < 0 && cnt[c] > 0) { NkColor q = GameColor(c); remap[c] = remap[NearestGameColor(q.r, q.g, q.b, keep)]; }
        for (int i = 0; i < n; ++i) if (g[i] != EMPTY) c_[i] = (uint8)remap[g[i]];
        time_ = 0; Recount(); UpdateReach();
        return np > 0;
    }

    // ---- Grille EMBARQUÉE (05/11/2026) : les cases sont déjà faites, à leur taille exacte et dans les couleurs de la palette de
    // jeu (tools/embarquer_niveaux.py). Aucun rééchantillonnage, aucune quantification : on copie.
    // cases[y * w + x] = 255 (vide) ou l'indice dans `pal` ; pal[k] = indice dans GameColor.
    bool LoadFromGrid(int w, int h, const unsigned char* cases, int nPal, const unsigned char* pal) {
        if (w <= 0 || h <= 0 || nPal <= 0 || nPal > MAX_PAL) return false;
        gw = w; gh = h; const int n = w * h;
        c_.Resize(n); cool_.Resize(n); acc_.Clear();
        np = nPal; for (int k = 0; k < nPal; ++k) palette[k] = GameColor(pal[k]);
        for (int i = 0; i < n; ++i) { c_[i] = cases[i] == 255 ? EMPTY : (uint8)cases[i]; cool_[i] = 0; }
        time_ = 0; Recount(); UpdateReach();
        return true;
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
    // L'accessibilité est TOUJOURS À JOUR quand on la lit (règle de Rodolf du 05/11/2026 : « pas de valeur vieille de
    // 0,2 s ») : retirer un bloc lève le drapeau `dirty_`, et la première lecture suivante refait le remplissage.
    // On ne le refait donc qu'une fois par changement, jamais à chaque lecture.
    void  EnsureReach() const { if (dirty_) const_cast<Board*>(this)->UpdateReach(); }
    bool  Accessible(int i) const { EnsureReach(); return acc_.Size() ? acc_[i] != 0 : Exposed(i); }
    void  UpdateReach() {
        dirty_ = false;
        int n = gw * gh; if (!n) return;
        acc_.Resize(n);
        NkVector<uint8> reach; reach.Resize(n); NkVector<int> st;               // st : pile du remplissage
        for (int i = 0; i < n; ++i) reach[i] = 0;
        for (int i = 0; i < n; ++i) { int x = i % gw, y = i / gw; if (c_[i] == EMPTY && (x == 0 || y == 0 || x == gw - 1 || y == gh - 1)) { reach[i] = 1; st.PushBack(i); } }
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
        while (st.Size() > 0) {
            int i = st[st.Size() - 1]; st.PopBack(); int x = i % gw, y = i / gw;
            for (int k = 0; k < 4; ++k) { int nx = x + dx[k], ny = y + dy[k]; if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                int j = ny * gw + nx; if (c_[j] == EMPTY && !reach[j]) { reach[j] = 1; st.PushBack(j); } }
        }
        for (int i = 0; i < n; ++i) {
            acc_[i] = 0; if (c_[i] == EMPTY) continue; int x = i % gw, y = i / gw;
            for (int k = 0; k < 4 && !acc_[i]; ++k) { int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) acc_[i] = 1; else if (reach[ny * gw + nx]) acc_[i] = 1; }
        }
    }
    // ---- Chemin (points de passage monde) jusqu'à une cellule d'accès du bloc `target`, ou jusqu'à un point.
    int FindPathToCell(float sx, float sy, int target, NkVec2f* out, int maxN) const { return Search(sx, sy, target, 0, 0, out, maxN); }
    int FindPathToPoint(float sx, float sy, float tx, float ty, NkVec2f* out, int maxN) const { return Search(sx, sy, -1, tx, ty, out, maxN); }
    bool  Present(int i, int color) const { return i >= 0 && i < gw * gh && c_[i] == color; }
    void  Cooldown(int i, float sec) { if (i >= 0 && i < gw * gh) cool_[i] = time_ + sec; }
    int   Take(int i) { int c = c_[i]; if (c != EMPTY) { c_[i] = EMPTY; --left_[c]; dirty_ = true; } return c; }   // retire un bloc
    void  ClosestOnCell(int i, float px, float py, float& qx, float& qy) const {
        float x0 = CellX(i) - cs * .5f, y0 = CellY(i) - cs * .5f;
        qx = NkClamp(px, x0, x0 + cs); qy = NkClamp(py, y0, y0 + cs);
    }

    // ---- Entity ---------------------------------------------------------------
    void Update(float dt) override { time_ += dt; }

    // Collision cercle / cases pleines : pour chaque case voisine, on cherche le point de la case le plus proche
    // du centre (borne du centre dans le carré) ; si la distance est inférieure au rayon, on repousse le cercle
    // le long de la normale (centre - point) de la profondeur de pénétration (r - d).
    // COLLISION FOURMI / CUBES. Une fourmi de rayon 8 * cs / 11 faisait 1,45 case de large : elle ne tenait pas dans un couloir
    // d'une case, se coinçait contre les coins, et le banc mesurait des blocages définitifs (d'où un drapeau resté un temps à
    // faux, les fourmis marchant alors sur les cubes). À cs / 17 (Gameplay::Init) elle fait 0,94 case et passe : collision
    // active, 25 niveaux sur 25 au banc. Si on regrossit les fourmis, repasser le banc.
    bool collideAnts = true;
    bool ResolveCircle(float& cx, float& cy, float r, float& nx, float& ny) override {
        if (!collideAnts) return false;
        r = NkMin(r, cs * 0.42f);                                         // rayon de collision réduit : passe dans les rainures d'1 cellule
        bool hit = false;
        int ix0 = (int)((cx - r - ox) / cs) - 1, ix1 = (int)((cx + r - ox) / cs) + 1;
        int iy0 = (int)((cy - r - oy) / cs) - 1, iy1 = (int)((cy + r - oy) / cs) + 1;
        for (int pass = 0; pass < 2; ++pass)
            for (int y = NkMax(iy0, 0); y <= iy1 && y < gh; ++y) for (int x = NkMax(ix0, 0); x <= ix1 && x < gw; ++x) {
                if (c_[y * gw + x] == EMPTY) continue;
                float qx, qy; ClosestOnCell(y * gw + x, cx, cy, qx, qy);
                float dx = cx - qx, dy = cy - qy, d = NkVec2f(dx, dy).Len();
                if (d >= r) continue;
                if (d > 1e-4f) { nx = dx / d; ny = dy / d; }
                else { float ex = cx - CellX(y * gw + x), ey = cy - CellY(y * gw + x), l = NkMax(1e-4f, NkVec2f(ex, ey).Len()); nx = ex / l; ny = ey / l; d = 0; }
                cx += nx * (r - d); cy += ny * (r - d); hit = true;
            }
        return hit;
    }

    void DrawShadow(Dl& dl, const NkView25D& cam) const override {   // ombre portée des cubes de bord
        if (c_.Size() == 0) return;
        for (int cy = 0; cy < gh; ++cy) for (int cx = 0; cx < gw; ++cx) {
            if (c_[cy * gw + cx] == EMPTY) continue;
            if (At(cx + 1, cy) != EMPTY && At(cx, cy + 1) != EMPTY) continue;
            float x0 = ox + cx * cs, y0 = oy + cy * cs;
            NkVec2f q[4] = { cam.ShadowPos(x0, y0, ch), cam.ShadowPos(x0 + cs, y0, ch), cam.ShadowPos(x0 + cs, y0 + cs, ch), cam.ShadowPos(x0, y0 + cs, ch) };
            dl.AddConvexPolyFilled(q, 4, NkColor(0, 0, 0, 45));
        }
        // Ombre de CONTACT : sous chaque arête avant exposée, une fine bande sombre sur l'assiette (là où le cube
        // touche le plateau, la lumière ne passe pas). Elle ancre le cube au sol et l'empêche de « flotter ».
        for (int cy = 0; cy < gh; ++cy) for (int cx = 0; cx < gw; ++cx) {
            if (c_[cy * gw + cx] == EMPTY || At(cx, cy + 1) != EMPTY) continue;
            float x0 = ox + cx * cs, y1 = oy + (cy + 1) * cs, d = cs * 0.30f;
            NkVec2f q[4] = { cam.Project(x0 - 1.f, y1, 0), cam.Project(x0 + cs + 1.f, y1, 0), cam.Project(x0 + cs + 2.f, y1 + d, 0), cam.Project(x0 - 2.f, y1 + d, 0) };
            dl.AddConvexPolyFilled(q, 4, NkColor(40, 30, 20, 50));
        }
    }

    void Emit(RenderQueue& q, const NkView25D& cam) const override {
        if (c_.Size() == 0) return;
        for (int i = 0; i < gw * gh; ++i)
            if (c_[i] != EMPTY) q.Custom(cam.Depth(0, CellY(i), ch), static_cast<const ICustomDraw*>(this), i);
    }

    // Cube « tendre » (05/11/2026), toujours aligné sur les axes : la caméra regarde le long de y, donc les faces
    // gauche et droite sont de profil (invisibles) et il reste le DESSUS et la face AVANT. Pour qu'il ne paraisse
    // pas plat, on empile cinq effets, du dessous vers le dessus :
    //   1. face avant en deux bandes : le tiers bas est plus sombre (l'ombre de l'assiette remonte sur le cube) ;
    //   2. dessus en OCTOGONE (coins coupés) : le cube paraît arrondi, comme du fromage ou de la gelée ;
    //   3. un octogone intérieur plus clair (biseau) : la face est légèrement bombée ;
    //   4. OCCLUSION entre voisins : un filet sombre sur chaque arête de dessus partagée avec un autre cube
    //      (la fente où la lumière n'entre pas) ;
    //   5. REFLET : un filet clair sur l'arête HAUTE, seulement si aucun cube n'est derrière (arête exposée,
    //      donc éclairée de plein fouet par la lumière venue du haut).
    // La teinte du dessus (x 1.04) et de la face avant (x 0.70) suit la même loi que Box::DrawBody : dessus plus clair,
    // côté plus sombre, pour qu'un cube posé sur le plateau et un cube porté par une fourmi se ressemblent.
    void DrawPiece(Dl& dl, const NkView25D& cam, int i) const override {
        int cx = i % gw, cy = i / gw; NkColor col = palette[c_[i]];
        float x0 = ox + cx * cs, y0 = oy + cy * cs, g = 0.4f;
        if (At(cx, cy + 1) == EMPTY) {
            NkVec2f F[4] = { cam.Project(x0 + g, y0 + cs, 0), cam.Project(x0 + cs - g, y0 + cs, 0),
                             cam.Project(x0 + cs - g, y0 + cs, ch), cam.Project(x0 + g, y0 + cs, ch) };
            dl.AddConvexPolyFilled(F, 4, col.Shade(0.72f));
            NkVec2f B[4] = { F[0], F[1], cam.Project(x0 + cs - g, y0 + cs, ch * .34f), cam.Project(x0 + g, y0 + cs, ch * .34f) };
            dl.AddConvexPolyFilled(B, 4, col.Shade(0.56f));                  // bande basse : ombre de contact remontée
        }
        const float c = cs * 0.05f, b = cs * 0.15f, cb = c * 0.6f;             // coins presque droits (5 % de la case), bevel mince
        NkVec2f T[8] = { cam.Project(x0 + g + c, y0, ch), cam.Project(x0 + cs - g - c, y0, ch), cam.Project(x0 + cs - g, y0 + c, ch),
                         cam.Project(x0 + cs - g, y0 + cs - c, ch), cam.Project(x0 + cs - g - c, y0 + cs, ch), cam.Project(x0 + g + c, y0 + cs, ch),
                         cam.Project(x0 + g, y0 + cs - c, ch), cam.Project(x0 + g, y0 + c, ch) };
        dl.AddConvexPolyFilled(T, 8, col.Lerp(NkColor(255, 255, 255, 255), 0.04f));
        NkVec2f I[8] = { cam.Project(x0 + b + cb, y0 + b, ch), cam.Project(x0 + cs - b - cb, y0 + b, ch), cam.Project(x0 + cs - b, y0 + b + cb, ch),
                         cam.Project(x0 + cs - b, y0 + cs - b - cb, ch), cam.Project(x0 + cs - b - cb, y0 + cs - b, ch), cam.Project(x0 + b + cb, y0 + cs - b, ch),
                         cam.Project(x0 + b, y0 + cs - b - cb, ch), cam.Project(x0 + b, y0 + b + cb, ch) };
        dl.AddConvexPolyFilled(I, 8, col.Lerp(NkColor(255, 255, 255, 255), 0.10f));
        // Occlusion : filet sombre sur chaque arête partagée avec un voisin (haut, droite, bas, gauche).
        const NkColor crease(0, 0, 0, 34);
        const float lw = 0.9f * cam.Scale();
        if (At(cx, cy - 1) != EMPTY) dl.AddLine(T[7], T[0], crease, lw);
        if (At(cx + 1, cy) != EMPTY) dl.AddLine(T[2], T[3], crease, lw);
        if (At(cx, cy + 1) != EMPTY) dl.AddLine(T[4], T[5], crease, lw);
        if (At(cx - 1, cy) != EMPTY) dl.AddLine(T[6], T[7], crease, lw);
        // Reflet sur l'arête haute exposée.
        { dl.AddLine(NkVec2f(T[7].x + cs * .1f * cam.Scale(), T[0].y + 0.9f * cam.Scale()), NkVec2f(T[2].x - cs * .1f * cam.Scale(), T[0].y + 0.9f * cam.Scale()), NkColor(255, 255, 255, 150), 1.1f * cam.Scale()); }   // liseré clair sur l'arête haute de CHAQUE cube
    }

    // Ancien dessin du cube (04/11/2026), gardé et non appelé : « on ne supprime pas ». Raison du retrait : dessus
    // carré sans reflet ni occlusion, le plateau paraissait plat.
    void DrawPieceAncien(Dl& dl, const NkView25D& cam, int i) const {
        int cx = i % gw, cy = i / gw; NkColor col = palette[c_[i]];
        float x0 = ox + cx * cs, y0 = oy + cy * cs, g = 0.3f;   // jeu en x seulement : pas de trait entre rangées
        if (At(cx, cy + 1) == EMPTY) {                                       // face avant visible seulement si rien devant
            NkVec2f F[4] = { cam.Project(x0 + g, y0 + cs, 0), cam.Project(x0 + cs - g, y0 + cs, 0),
                          cam.Project(x0 + cs - g, y0 + cs, ch), cam.Project(x0 + g, y0 + cs, ch) };
            dl.AddConvexPolyFilled(F, 4, col.Shade(0.72f));
        }
        NkVec2f T[4] = { cam.Project(x0 + g, y0, ch), cam.Project(x0 + cs - g, y0, ch),
                      cam.Project(x0 + cs - g, y0 + cs, ch), cam.Project(x0 + g, y0 + cs, ch) };
        dl.AddConvexPolyFilled(T, 4, col.Shade(1.08f));
        float b = cs * 0.2f;                                                 // biseau intérieur plus clair
        NkVec2f I[4] = { cam.Project(x0 + b, y0 + b, ch), cam.Project(x0 + cs - b, y0 + b, ch),
                      cam.Project(x0 + cs - b, y0 + cs - b, ch), cam.Project(x0 + b, y0 + cs - b, ch) };
        dl.AddConvexPolyFilled(I, 4, col.Shade(1.18f));
    }

private:
    // Chemin : plus court chemin en nombre de cases (parcours en largeur, NkGridPath) puis lissage par ligne de vue.
    // La grille est entourée d'une marge libre de M cases (les fourmis tournent autour du plateau). Les blocs
    // sont les cases bloquées. Le but d'un bloc est l'une de ses 4 cases voisines libres : on essaie les quatre
    // et on garde le plus court.
    int Search(float sx, float sy, int target, float tx, float ty, NkVec2f* out, int maxN) const {
        const int M = 8, PW = gw + 2 * M, PH = gh + 2 * M;
        nav::NkGridPath grid; grid.Init(PW, PH);
        for (int y = 0; y < gh; ++y) for (int x = 0; x < gw; ++x) if (c_[y * gw + x] != EMPTY) grid.SetBlocked(x + M, y + M, true);
        auto cellOf = [&](float wx, float wy, int& px, int& py) {
            px = NkClamp((int)((wx - ox) / cs + 1000.f) - 1000 + M, 0, PW - 1); py = NkClamp((int)((wy - oy) / cs + 1000.f) - 1000 + M, 0, PH - 1); };
        int spx, spy; cellOf(sx, sy, spx, spy); grid.SetBlocked(spx, spy, false);   // le départ est toujours libre
        NkVector<nav::NkGridCell> path;
        int gx = -1, gy = -1;
        if (target >= 0) {
            int x = target % gw, y = target / gw; const int D[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
            usize bestLen = 0; NkVector<nav::NkGridCell> cur;
            for (int k = 0; k < 4; ++k) {
                int ax = x + D[k][0] + M, ay = y + D[k][1] + M;
                if (grid.IsBlocked(ax, ay) || !grid.FindPath(spx, spy, ax, ay, cur, false)) continue;
                if (gx < 0 || cur.Size() < bestLen) { bestLen = cur.Size(); gx = ax; gy = ay; }
            }
        } else { cellOf(tx, ty, gx, gy); grid.SetBlocked(gx, gy, false); }
        if (gx < 0 || !grid.FindPath(spx, spy, gx, gy, path, true)) return 0;
        int cnt = 0;
        for (usize i = 0; i < path.Size() && cnt < maxN; ++i)
            out[cnt++] = NkVec2f(ox + ((path[i].x - M) + .5f) * cs, oy + ((path[i].y - M) + .5f) * cs);
        return cnt;
    }
    void Recount() { for (int i = 0; i < MAX_PAL; ++i) left_[i] = 0; for (int i = 0; i < gw * gh; ++i) if (c_[i] != EMPTY) ++left_[c_[i]]; }

    NkVector<uint8> c_; NkVector<float> cool_; NkVector<uint8> acc_; int left_[MAX_PAL] = {}; float time_ = 0; bool dirty_ = false;
};

// =============================================================================
//  Hole : trou de nid (décor au sol). Les fourmis y apportent les blocs.
// =============================================================================
class Hole : public Entity {
public:
    float rx = 22, ry = 14;
    Hole() { kind = KIND_HOLE; }
    void Update(float) override {}
    // FOURMILIÈRE (05/11/2026) : un monticule de terre à étages, des grains éparpillés autour et, au centre,
    // l'ouverture sombre où les fourmis disparaissent. Quatre ellipses empilées, de plus en plus petites et claires
    // et décalées vers le haut (la lumière vient du haut) donnent la bosse ; l'ouverture est une ellipse sombre
    // posée en avant de la bosse, avec un cercle encore plus noir pour la profondeur. Les grains sont placés par une
    // suite pseudo-aléatoire de graine fixe (donc identique à chaque trame, rien ne scintille).
    void DrawShadow(Dl& dl, const NkView25D& cam) const override {
        NkVec2f c = cam.Project(x, y, 0);
        const float rx = this->rx * cam.Scale(), ry = this->ry * cam.Scale();     // les rayons du nid sont en unités de plan : on les passe en pixels
        const float k = rx / 36.f;
        dl.AddEllipseFilled(NkVec2f(c.x + rx * .08f, c.y + ry * .22f), rx * 1.42f, ry * 1.38f, NkColor(40, 60, 20, 60));          // ombre au sol
        dl.AddEllipseFilled(NkVec2f(c.x, c.y + ry * .08f), rx * 1.22f, ry * 1.18f, NkColor(138, 96, 60, 255));                      // jupe de terre
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .06f), rx * 1.08f, ry * 1.04f, NkColor(171, 123, 80, 255));
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .22f), rx * .92f, ry * .88f, NkColor(193, 146, 98, 255));
        dl.AddEllipseFilled(NkVec2f(c.x - rx * .08f, c.y - ry * .38f), rx * .74f, ry * .66f, NkColor(210, 166, 118, 255));           // sommet éclairé
        NkLcg32 r; r.Seed(77u);
        for (int i = 0; i < 22; ++i) {                                                                                              // grains autour
            float a = (float)r.NextUInt32(628) * 0.01f, d = 1.12f + (float)r.NextUInt32(40) * 0.01f, g = (1.1f + (float)r.NextUInt32(12) * 0.1f) * k;
            int t = (int)r.NextUInt32(3); NkColor gc = t == 0 ? NkColor(150, 106, 68, 255) : (t == 1 ? NkColor(189, 143, 96, 255) : NkColor(120, 82, 52, 255));
            dl.AddEllipseFilled(NkVec2f(c.x + NkCos(a) * rx * d, c.y + NkSin(a) * ry * d * 1.02f), g, g * .8f, gc);
        }
        dl.AddEllipseFilled(NkVec2f(c.x, c.y + ry * .02f), rx * .58f, ry * .64f, NkColor(94, 62, 40, 255));                         // lèvre de l'ouverture
        dl.AddEllipseFilled(NkVec2f(c.x, c.y), rx * .52f, ry * .57f, NkColor(46, 28, 20, 255));                                      // ouverture
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .08f), rx * .36f, ry * .38f, NkColor(20, 11, 8, 255));                           // fond noir
    }
    void Emit(RenderQueue&, const NkView25D&) const override {}
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
            int n = NkMax(1, (int)((float)total * board_->Remaining(c) / all + 0.5f));
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
                    if (NkVec2f(qx - a->x, qy - a->y).Len() <= a->Radius() + 3.f) { Grab(a); continue; }
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
