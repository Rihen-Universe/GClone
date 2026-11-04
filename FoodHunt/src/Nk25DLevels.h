// =============================================================================
// Nk25DLevels.hpp — Chargement d'images et de SOUS-IMAGES pour fabriquer des niveaux.
//
// Chaîne de traitement (de la planche d'icônes jusqu'aux blocs du plateau) :
//
//   fichier ──IImageLoader──► ImageData (RGBA)         [décodage : NkImage chez vous]
//           ──SpriteSheet───► liste de rectangles       [découpe : grille manuelle OU détection auto]
//           ──Extract──────► sous-image RGBA détourée   [fond retiré par remplissage depuis les bords]
//           ──Board::LoadFromRGBA─► grille de blocs     [réduction + quantification couleurs]
//
// "N'importe quelle image ?"  Oui : tout ce qui est décodable en RGBA (PNG, JPEG, BMP...).
//   - PNG avec transparence : la transparence est utilisée telle quelle.
//   - JPEG / image opaque   : le fond (estimé sur les bords) est retiré automatiquement.
//   - Le résultat est le meilleur avec des formes nettes sur fond clair uni (icônes, logos,
//     pixel-art). Une photo marche, mais la palette réduite (6-8 couleurs) la "postérise".
// Sans STL.
// =============================================================================
#pragma once
#include "Nk25DBoard.h"
#include <stdio.h>

namespace nk25d {

// ---- Image RGBA possédée (4 octets/pixel, ligne par ligne, origine en haut à gauche) ---------
struct ImageData {
    unsigned char* rgba = nullptr; int w = 0, h = 0;
    ImageData() = default;
    ~ImageData() { Free(); }
    ImageData(const ImageData&) = delete; ImageData& operator=(const ImageData&) = delete;   // propriétaire unique
    bool Alloc(int W, int H) { Free(); if (W <= 0 || H <= 0) return false; rgba = new unsigned char[(size_t)W * H * 4]; w = W; h = H; return true; }
    void Free() { delete[] rgba; rgba = nullptr; w = h = 0; }
};

// ---- Décodeur d'image : INTERFACE. Votre adaptateur NkImage l'implémente (voir README) ---------
// On isole le décodage derrière une interface pour que ce module ne dépende d'aucun format :
// c'est le principe d'inversion de dépendance (le haut niveau ne connaît pas le bas niveau).
struct IImageLoader {
    virtual ~IImageLoader() {}
    virtual bool Load(const char* path, ImageData& out) = 0;      // doit remplir out (RGBA) ; true si succès
};

// Décodeur PPM (P6) minimal : sert aux tests hors moteur, sans aucune bibliothèque.
class PpmLoader : public IImageLoader {
public:
    bool Load(const char* path, ImageData& out) override {
        FILE* f = fopen(path, "rb"); if (!f) return false;
        int w = 0, h = 0, mx = 0; if (fscanf(f, "P6 %d %d %d", &w, &h, &mx) != 3 || mx != 255) { fclose(f); return false; }
        fgetc(f);                                                  // un seul espace/retour après l'en-tête
        if (!out.Alloc(w, h)) { fclose(f); return false; }
        for (int i = 0; i < w * h; ++i) { unsigned char px[3]; if (fread(px, 1, 3, f) != 3) { fclose(f); return false; }
            out.rgba[i*4] = px[0]; out.rgba[i*4+1] = px[1]; out.rgba[i*4+2] = px[2]; out.rgba[i*4+3] = 255; }
        fclose(f); return true;
    }
};

struct SubRect { int x = 0, y = 0, w = 0, h = 0; };

// =============================================================================
//  SpriteSheet : une grande image + une liste de rectangles (les "sous-images").
// =============================================================================
class SpriteSheet {
public:
    enum { MAX_RECTS = 512 };

    bool Load(IImageLoader& loader, const char* path) { if (!loader.Load(path, img_)) return false; EstimateBg(); return true; }
    bool SetImage(const unsigned char* rgba, int w, int h) {          // copie depuis un buffer existant
        if (!img_.Alloc(w, h)) return false; for (size_t i = 0; i < (size_t)w * h * 4; ++i) img_.rgba[i] = rgba[i]; EstimateBg(); return true;
    }
    int  Count() const { return n_; }
    const SubRect& Rect(int i) const { return r_[i]; }
    int  Width() const { return img_.w; }
    int  Height() const { return img_.h; }

    // ---- Découpe MANUELLE : grille régulière --------------------------------------------------
    // cols x rows cellules dans la zone (image moins marges en pixels). `inset*` = fractions (0..1) de
    // CHAQUE cellule ignorées sur le bord : utile pour exclure une légende ou une case à cocher
    // (ex. planche 2 : le texte est sous l'icône => insetBottom = 0.3).
    // Les rectangles sont numérotés dans l'ordre de lecture (gauche->droite, haut->bas).
    int SetGrid(int cols, int rows, int mL = 0, int mT = 0, int mR = 0, int mB = 0,
                float insL = 0, float insT = 0, float insR = 0, float insB = 0) {
        n_ = 0; if (cols <= 0 || rows <= 0 || !img_.rgba) return 0;
        float cw = (img_.w - mL - mR) / (float)cols, ch = (img_.h - mT - mB) / (float)rows;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols && n_ < MAX_RECTS; ++c) {
            float x0 = mL + c * cw + cw * insL, y0 = mT + r * ch + ch * insT;
            float x1 = mL + (c + 1) * cw - cw * insR, y1 = mT + (r + 1) * ch - ch * insB;
            r_[n_++] = { (int)x0, (int)y0, m::Max(1, (int)(x1 - x0)), m::Max(1, (int)(y1 - y0)) };
        }
        return n_;
    }
    void AddRect(int x, int y, int w, int h) { if (n_ < MAX_RECTS) r_[n_++] = { x, y, w, h }; }   // rectangle explicite

    // ---- Découpe AUTOMATIQUE : profils de projection ------------------------------------------
    // Idée (segmentation par histogrammes de projection, classique en analyse de documents) :
    //   1. Pour chaque ligne de pixels, on compte les pixels "non-fond". Les lignes vides forment des
    //      GOUTTIÈRES horizontales ; entre deux gouttières = une bande de ligne d'icônes.
    //   2. Dans chaque bande, même chose en colonnes : les colonnes vides séparent les icônes.
    // Hypothèse : les icônes sont séparées par du fond. Un petit bruit (JPEG, filigrane clair) est
    // absorbé par `bgTol` (tolérance de couleur) et `minGap` (une gouttière plus fine est ignorée).
    int AutoDetect(int minSize = 12, int bgTol = 55, int minGap = 3) {
        n_ = 0; if (!img_.rgba) return 0; const int W = img_.w, H = img_.h;
        bool* rowOn = new bool[H];
        for (int y = 0; y < H; ++y) { int cnt = 0; for (int x = 0; x < W; ++x) if (!IsBg(img_.rgba + ((size_t)y * W + x) * 4, bgTol)) ++cnt; rowOn[y] = cnt >= 3; }
        int sy[64], ey[64], nb = Spans(rowOn, H, minSize, minGap, sy, ey, 64);
        bool* colOn = new bool[W];
        for (int b = 0; b < nb; ++b) {
            for (int x = 0; x < W; ++x) { int cnt = 0; for (int y = sy[b]; y <= ey[b]; ++y) if (!IsBg(img_.rgba + ((size_t)y * W + x) * 4, bgTol)) ++cnt; colOn[x] = cnt >= 2; }
            int sx[64], ex[64], nc = Spans(colOn, W, minSize, minGap, sx, ex, 64);
            for (int c = 0; c < nc && n_ < MAX_RECTS; ++c) r_[n_++] = { sx[c], sy[b], ex[c] - sx[c] + 1, ey[b] - sy[b] + 1 };
        }
        delete[] rowOn; delete[] colOn; return n_;
    }

    // ---- Extraction d'une sous-image détourée ---------------------------------------------------
    // 1. copie du rectangle ; 2. FOND = pixels proches de la couleur de fond ET reliés au bord par un
    //    chemin de pixels de fond (remplissage "flood fill") -> alpha 0. Les blancs INTÉRIEURS à l'icône
    //    (ex. blanc d'œuf) ne sont pas reliés au bord, donc ils restent opaques ;
    // 3. recadrage serré sur la boîte englobante du contenu (+1 px).
    bool Extract(int idx, ImageData& out, int bgTol = 55) const {
        if (idx < 0 || idx >= n_) return false;
        SubRect r = r_[idx]; r.x = m::Clamp(r.x, 0, img_.w - 1); r.y = m::Clamp(r.y, 0, img_.h - 1);
        r.w = m::Min(r.w, img_.w - r.x); r.h = m::Min(r.h, img_.h - r.y);
        const int RW = r.w, RH = r.h; if (RW < 2 || RH < 2) return false;
        unsigned char* a = new unsigned char[RW * RH]; int* st = new int[RW * RH]; int sp = 0;
        for (int i = 0; i < RW * RH; ++i) a[i] = 255;
        auto px = [&](int x, int y) { return img_.rgba + ((size_t)(r.y + y) * img_.w + (r.x + x)) * 4; };
        auto seed = [&](int x, int y) { int k = y * RW + x; if (a[k] && IsBg(px(x, y), bgTol)) { a[k] = 0; st[sp++] = k; } };
        for (int x = 0; x < RW; ++x) { seed(x, 0); seed(x, RH - 1); }
        for (int y = 0; y < RH; ++y) { seed(0, y); seed(RW - 1, y); }
        while (sp > 0) {                                               // propagation 4-connexe
            int k = st[--sp], x = k % RW, y = k / RW; const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) { int nx = x + dx[d], ny = y + dy[d]; if (nx >= 0 && ny >= 0 && nx < RW && ny < RH) seed(nx, ny); }
        }
        int x0 = RW, y0 = RH, x1 = -1, y1 = -1;                        // boîte englobante du contenu
        for (int y = 0; y < RH; ++y) for (int x = 0; x < RW; ++x) if (a[y * RW + x]) { x0 = m::Min(x0, x); y0 = m::Min(y0, y); x1 = m::Max(x1, x); y1 = m::Max(y1, y); }
        bool ok = x1 >= x0 && y1 >= y0;
        if (ok) {
            x0 = m::Max(0, x0 - 1); y0 = m::Max(0, y0 - 1); x1 = m::Min(RW - 1, x1 + 1); y1 = m::Min(RH - 1, y1 + 1);
            ok = out.Alloc(x1 - x0 + 1, y1 - y0 + 1);
            for (int y = y0; ok && y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
                unsigned char* d = out.rgba + ((size_t)(y - y0) * out.w + (x - x0)) * 4; const unsigned char* s = px(x, y);
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = a[y * RW + x];
            }
        }
        delete[] a; delete[] st; return ok;
    }

private:
    // Pixel de fond ? : transparent, ou à moins de `tol` de la couleur de fond (distance euclidienne en RGB).
    bool IsBg(const unsigned char* p, int tol) const {
        if (p[3] < 128) return true;
        int dr = p[0] - bg_[0], dg = p[1] - bg_[1], db = p[2] - bg_[2];
        return dr * dr + dg * dg + db * db < tol * tol;
    }
    // Couleur de fond = MÉDIANE par canal des pixels du pourtour (robuste à quelques icônes qui touchent le bord).
    void EstimateBg() {
        int hist[3][256] = {}; int cnt = 0; const int W = img_.w, H = img_.h;
        auto add = [&](int x, int y) { const unsigned char* p = img_.rgba + ((size_t)y * W + x) * 4; for (int c = 0; c < 3; ++c) ++hist[c][p[c]]; ++cnt; };
        for (int x = 0; x < W; ++x) { add(x, 0); add(x, H - 1); }
        for (int y = 1; y < H - 1; ++y) { add(0, y); add(W - 1, y); }
        for (int c = 0; c < 3; ++c) { int acc = 0, v = 0; while (v < 255 && acc + hist[c][v] < cnt / 2) { acc += hist[c][v]; ++v; } bg_[c] = (unsigned char)v; }
    }
    // Regroupe les indices `on[i]==true` consécutifs en intervalles [s,e]. Deux intervalles séparés par moins
    // de `minGap` positions vides sont fusionnés ; les intervalles plus courts que `minSize` sont jetés.
    static int Spans(const bool* on, int n, int minSize, int minGap, int* s, int* e, int cap) {
        int cnt = 0, i = 0;
        while (i < n && cnt < cap) {
            while (i < n && !on[i]) ++i; if (i >= n) break;
            int start = i, last = i, gap = 0;
            for (; i < n; ++i) { if (on[i]) { last = i; gap = 0; } else if (++gap >= minGap) break; }
            if (last - start + 1 >= minSize) { s[cnt] = start; e[cnt] = last; ++cnt; }
        }
        return cnt;
    }
    ImageData img_; SubRect r_[MAX_RECTS]; int n_ = 0; unsigned char bg_[3] = {255, 255, 255};
};

// =============================================================================
//  LevelLibrary : suite ordonnée de niveaux (chaque niveau = une sous-image d'une planche).
//  "Charger l'image suivante" = LoadLevel(i + 1).
// =============================================================================
class LevelLibrary {
public:
    enum { MAX_LEVELS = 512, MAX_SHEETS = 16 };
    ~LevelLibrary() { for (int i = 0; i < ns_; ++i) delete sheets_[i]; }
    LevelLibrary() = default; LevelLibrary(const LevelLibrary&) = delete; LevelLibrary& operator=(const LevelLibrary&) = delete;

    SpriteSheet* NewSheet() { return ns_ < MAX_SHEETS ? (sheets_[ns_++] = new SpriteSheet()) : nullptr; }   // la bibliothèque possède la planche
    int  AddAll(SpriteSheet* s) { for (int i = 0; i < s->Count() && n_ < MAX_LEVELS; ++i) refs_[n_++] = { s, i }; return n_; }   // un niveau par rectangle
    int  AddOne(SpriteSheet* s, int idx) { if (n_ < MAX_LEVELS && idx >= 0 && idx < s->Count()) refs_[n_++] = { s, idx }; return n_; }
    int  Count() const { return n_; }

    // Charge le niveau i dans `b`. `blocksAcross` = nombre de blocs sur la plus grande dimension de l'image
    // (40 => ~800 à 1500 blocs) ; `maxColors` = taille de la palette (nombre de couleurs de fourmis).
    bool LoadLevel(int i, Board& b, int blocksAcross = 40, int maxColors = 6) const {
        if (i < 0 || i >= n_) return false;
        ImageData img; if (!refs_[i].s->Extract(refs_[i].idx, img)) return false;
        // Conserver le rapport d'aspect : la plus grande dimension reçoit `blocksAcross` blocs.
        int gridW = img.w >= img.h ? blocksAcross : m::Max(6, (int)(blocksAcross * (float)img.w / img.h + .5f));
        gridW = m::Min(gridW, img.w);                                  // jamais plus de blocs que de pixels
        return b.LoadFromRGBA(img.rgba, img.w, img.h, gridW, maxColors, false);   // fond déjà retiré (alpha)
    }
private:
    struct Ref { SpriteSheet* s; int idx; };
    Ref refs_[MAX_LEVELS]; int n_ = 0; SpriteSheet* sheets_[MAX_SHEETS]; int ns_ = 0;
};

} // namespace nk25d