// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DArt.h — L'habillage « pique-nique au jardin » de FoodHunt, dessiné ENTIÈREMENT par le code
// (NkGuiDrawList) : aucun fichier d'art, sauf les images des niveaux. Créé le 05/11/2026.
//
// Direction artistique (Rodolf : « conserver le style, donc l'idée et non les assets ») : une herbe tendre, une
// assiette de céramique claire qui porte l'image en cubes, une fourmilière de terre, des paniers d'osier teintés de
// la couleur des fourmis, un rayon de miel pour le stock, de petites icônes dessinées pour les bonus.
// Tout est exprimé en coordonnées LOGIQUES (voir Gameplay::SetView) : l'application met ensuite l'ensemble à
// l'échelle de l'écran en une fois. Les fonctions sont libres et sans état : elles peuvent se tester seules.
#pragma once
#include "Nk25D.h"

namespace nk25d {

// ---- Palette de l'interface (une seule source : changer ici change tout le jeu) --------------------------------
namespace pal {
    inline NkColor Creme()   { return NkColor(255, 247, 228, 255); }
    inline NkColor Brun()    { return NkColor(98, 62, 42, 255); }        // texte sur fond clair, contours
    inline NkColor Tomate()  { return NkColor(226, 76, 60, 255); }
    inline NkColor Moutarde(){ return NkColor(244, 190, 62, 255); }
    inline NkColor Feuille() { return NkColor(98, 170, 72, 255); }
    inline NkColor Ciel()    { return NkColor(98, 162, 222, 255); }
    inline NkColor Osier()   { return NkColor(214, 168, 96, 255); }
}

// ---- LA PALETTE DE JEU (Rodolf, 05/11 : « les couleurs ne vont pas ») --------------------------------------------
// Douze couleurs (creme et brun clair ajoutes le 05/11 pour les pates et les croutes), vives et bien SÉPARÉES. Chaque couleur d'une image de niveau est ramenée à la plus proche de celles-ci
// (Board::LoadFromImage) : un niveau n'a donc jamais deux couleurs proches, et la MÊME couleur exacte sert au cube, à la
// fourmi, au panier et à l'alvéole. Le blanc et le brun sombre y sont pour les images claires et sombres.
constexpr int kNbGameColors = 12;
inline NkColor GameColor(int i) {
    static const unsigned char t[kNbGameColors][3] = {
        {228, 52, 52}, {255, 146, 30}, {250, 214, 40}, {84, 190, 62}, {28, 190, 180},
        {52, 112, 232}, {146, 76, 204}, {250, 122, 190}, {245, 245, 240}, {104, 66, 40}, {252, 228, 176}, {196, 138, 80} };   // 8 blanc, 9 brun sombre (contour), 10 creme, 11 brun clair
    i = i < 0 ? 0 : (i >= kNbGameColors ? kNbGameColors - 1 : i);
    return NkColor(t[i][0], t[i][1], t[i][2], 255);
}
// Couleur la plus proche (distance « redmean », approximation de la perception : le rouge pèse plus quand le pixel est rouge).
inline int NearestGameColor(int r, int g, int b, unsigned mask = 0xffffffffu) {
    int best = 0; long bd = 0x7fffffffL;
    for (int i = 0; i < kNbGameColors; ++i) {
        if (!(mask & (1u << i))) continue;
        NkColor c = GameColor(i); long rm = (r + c.r) / 2, dr = r - c.r, dg = g - c.g, db = b - c.b;
        long d = (2 + rm / 256) * dr * dr + 4 * dg * dg + (2 + (255 - rm) / 256) * db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
// La couleur « mystère » : un gris-bleu sombre qu'aucune couleur de la palette ne rappelle.
inline NkColor MysteryColor() { return NkColor(58, 66, 92, 255); }
// Éclaircir sans changer de teinte : on mélange vers le blanc (Shade > 1 écrête chaque canal et vire au jaune/blanc).
inline NkColor Lighten(const NkColor& c, float t) { return c.Lerp(NkColor(255, 255, 255, 255), t); }

// ---- Petits outils -------------------------------------------------------------------------------------------
inline float Rad(float deg) { return deg * (NK_PI_F / 180.f); }

// Hexagone régulier « pointe en haut » de rayon R (distance centre-sommet). Sommets aux angles -90, -30, 30, 90...
// Pavage en rayon de miel : largeur sqrt(3) R, rangées espacées de 1.5 R, rangées impaires décalées d'une demi-largeur.
inline void HexPoly(Dl& dl, float cx, float cy, float R, const NkColor& c) {
    NkVec2f p[6];
    for (int k = 0; k < 6; ++k) { float a = Rad(60.f * k - 90.f); p[k] = NkVec2f(cx + R * NkCos(a), cy + R * NkSin(a)); }
    dl.AddConvexPolyLisse(p, 6, c);
}
// Test de clic dans l'hexagone « pointe en haut » (repère centré) : |dx| <= (sqrt(3)/2) R et |dy| <= R - |dx| / sqrt(3).
// La seconde inégalité est l'équation des deux arêtes obliques (pente 1/sqrt(3)) ; c'est exact, pas un cercle inscrit.
inline bool InHex(float x, float y, float cx, float cy, float R) {
    float dx = NkAbs(x - cx), dy = NkAbs(y - cy);
    return dx <= 0.8660254f * R && dy <= R - dx * 0.5773503f;
}

// Hexagone « sommets plats » (nid d'abeilles par COLONNES) : sommets aux angles 0, 60, 120... ; largeur 2 R, hauteur sqrt(3) R.
// Colonnes espacées de 1.5 R ; une colonne sur deux décalée d'une demi-hauteur ; DANS une colonne les alvéoles sont
// l'une sous l'autre, donc une alvéole qui monte dans sa colonne monte tout droit. `sx` écrase en largeur (bascule).
inline void HexPolyFlat(Dl& dl, float cx, float cy, float R, const NkColor& c, float sx = 1.f) {
    NkVec2f p[6];
    for (int k = 0; k < 6; ++k) { float a = Rad(60.f * k); p[k] = NkVec2f(cx + sx * R * NkCos(a), cy + R * NkSin(a)); }
    dl.AddConvexPolyLisse(p, 6, c);
}
// Test de clic, hexagone à sommets plats : |dy| <= (sqrt(3)/2) R et |dx| <= R - |dy| / sqrt(3) (arêtes obliques, pente sqrt(3)).
inline bool InHexFlat(float x, float y, float cx, float cy, float R) {
    float dx = NkAbs(x - cx), dy = NkAbs(y - cy);
    return dy <= 0.8660254f * R && dx <= R - dy * 0.5773503f;
}

// Étoile à cinq branches (non convexe : dix triangles issus du centre).
inline void Star(Dl& dl, float cx, float cy, float R, const NkColor& c) {
    NkVec2f p[10];
    for (int k = 0; k < 10; ++k) { float a = Rad(36.f * k - 90.f), r = (k & 1) ? R * 0.46f : R; p[k] = NkVec2f(cx + r * NkCos(a), cy + r * NkSin(a)); }
    for (int k = 0; k < 10; ++k) dl.AddTriangleFilled(NkVec2f(cx, cy), p[k], p[(k + 1) % 10], c);
}

// Rectangle arrondi centré avec ombre douce en dessous : le « bouton » de l'interface.
inline void Pill(Dl& dl, float cx, float cy, float w, float h, const NkColor& c, float rounding = -1.f) {
    float r = rounding < 0 ? h * .5f : rounding;
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f + h * .09f, w, h), c.Shade(0.62f), NkMin(r, NkMin(w, h) * .499f));
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h * .93f), c, NkMin(r, NkMin(w, h) * .499f));
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f + h * .08f, cy - h * .5f + h * .06f, w - h * .16f, NkMin(h * .34f, 16.f)), c.Shade(1.16f), NkMin(r, h * .17f));
}

// Carte (panneaux de victoire et d'échec, 05/11) : ombre douce centrée un peu plus bas (quatre rectangles de plus en plus
// grands et transparents, comme l'assiette), corps uni, simple filet clair en haut. Plus de reflet en demi-carte.
inline void Card(Dl& dl, float cx, float cy, float w, float h, const NkColor& c, float rounding) {
    for (int i = 4; i >= 1; --i) { float g = i * 4.f; dl.AddRectFilled(nkgui::NkRect(cx - w * .5f - g, cy - h * .5f - g + 10.f, w + 2 * g, h + 2 * g), NkColor(20, 40, 15, 26), rounding + g); }
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h), c, rounding);
    dl.AddRect(nkgui::NkRect(cx - w * .5f + 1.f, cy - h * .5f + 1.f, w - 2.f, h - 2.f), NkColor(255, 255, 255, 160), 2.f, rounding);
}

// ---- Fond : l'herbe --------------------------------------------------------------------------------------------
// (x0, y0)-(x1, y1) est la zone VISIBLE entière en coordonnées logiques : le fond couvre l'écran quelle que soit sa forme.
// 1. dégradé vertical doux (vert clair en haut, plus profond en bas) ;
// 2. brins d'herbe, trèfles et petites fleurs placés par une suite pseudo-aléatoire de graine fixe : identiques à chaque
//    trame (rien ne scintille) et discrets (alpha bas) pour ne pas gêner la lecture du jeu ;
// 3. vignettage : quatre bandes dégradées vert foncé -> transparent sur les bords.
inline void DrawGrass(Dl& dl, float x0, float y0, float x1, float y1) {
    const float w = x1 - x0, h = y1 - y0;
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y0, w, h), NkColor(176, 218, 118, 255), NkColor(176, 218, 118, 255), NkColor(116, 176, 84, 255), NkColor(116, 176, 84, 255));
    NkLcg32 r; r.Seed(20261105u);
    for (int i = 0; i < 70; ++i) {                                                    // brins : trois petits traits en éventail
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w, y = y0 + (float)r.NextUInt32(1000) * 0.001f * h;
        for (int k = -1; k <= 1; ++k) dl.AddLine(NkVec2f(x + k * 3.f, y), NkVec2f(x + k * 5.5f, y - 7.f - (float)(k * k) * 1.5f), NkColor(92, 150, 64, 70), 1.4f);
    }
    for (int i = 0; i < 26; ++i) {                                                    // trèfles : trois feuilles rondes + tige
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w, y = y0 + (float)r.NextUInt32(1000) * 0.001f * h, s = 5.f + (float)r.NextUInt32(30) * 0.1f;
        NkColor g(84, 148, 66, 95);
        dl.AddLine(NkVec2f(x, y + s), NkVec2f(x + 1.5f, y + s * 2.6f), g, 1.3f);
        dl.AddEllipseFilled(NkVec2f(x - s * .6f, y + s * .2f), s * .62f, s * .62f, g);
        dl.AddEllipseFilled(NkVec2f(x + s * .6f, y + s * .2f), s * .62f, s * .62f, g);
        dl.AddEllipseFilled(NkVec2f(x, y - s * .5f), s * .62f, s * .62f, g);
    }
    for (int i = 0; i < 16; ++i) {                                                    // fleurs : cinq pétales autour d'un coeur
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w, y = y0 + (float)r.NextUInt32(1000) * 0.001f * h, s = 3.2f + (float)r.NextUInt32(20) * 0.1f;
        int t = (int)r.NextUInt32(3); NkColor pc = t == 0 ? NkColor(255, 255, 255, 170) : (t == 1 ? NkColor(255, 220, 90, 170) : NkColor(255, 170, 190, 170));
        for (int k = 0; k < 5; ++k) dl.AddEllipseFilled(NkVec2f(x + s * 1.25f * NkCos(Rad(72.f * k)), y + s * 1.25f * NkSin(Rad(72.f * k))), s, s, pc);
        dl.AddEllipseFilled(NkVec2f(x, y), s * .8f, s * .8f, NkColor(244, 176, 40, 200));
    }
    const NkColor sombre(30, 70, 30, 80), nul(30, 70, 30, 0);                          // vignettage
    const float e = NkMin(w, h) * 0.20f;
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y0, w, e), sombre, sombre, nul, nul);
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y1 - e, w, e), nul, nul, sombre, sombre);
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y0, e, h), sombre, nul, nul, sombre);
    dl.AddRectFilledMultiColor(nkgui::NkRect(x1 - e, y0, e, h), nul, sombre, sombre, nul);
}

// ---- Nappe à carreaux (le bas de l'écran, derrière les boutons de bonus) --------------------------------------
// Le vichy : un fond crème, des bandes verticales rouges semi-transparentes, puis des bandes horizontales de même
// couleur ; là où elles se croisent le rouge s'additionne (carreau plus sombre), c'est ce qui fait le damier.
inline void DrawGingham(Dl& dl, float x0, float y0, float x1, float y1) {
    const float t = 16.f;
    dl.AddRectFilled(nkgui::NkRect(x0, y0 - 5.f, x1 - x0, 7.f), NkColor(30, 60, 20, 45));                 // ombre portée vers le haut
    dl.AddRectFilled(nkgui::NkRect(x0, y0, x1 - x0, y1 - y0), pal::Creme());
    const NkColor rouge(226, 76, 60, 120);
    for (float x = x0; x < x1; x += 2 * t) dl.AddRectFilled(nkgui::NkRect(x, y0, NkMin(t, x1 - x), y1 - y0), rouge);
    for (float y = y0; y < y1; y += 2 * t) dl.AddRectFilled(nkgui::NkRect(x0, y, x1 - x0, NkMin(t, y1 - y)), rouge);
    dl.AddRectFilled(nkgui::NkRect(x0, y0, x1 - x0, 3.f), NkColor(255, 255, 255, 170));                   // liseré clair au bord haut
}

// ---- L'assiette ------------------------------------------------------------------------------------------------
// Rectangle à grands coins ronds (rayon = 22 % du petit côté), de l'extérieur vers l'intérieur :
//   1. ombre portée douce sur l'herbe : quatre rectangles de plus en plus grands, de plus en plus transparents
//      (l'empilement d'alphas bas imite un flou gaussien) et décalés vers le bas-droite (la lumière vient d'en haut à gauche) ;
//   2. le bord : porcelaine crème, avec un reflet clair sur la moitié haute ;
//   3. le LISERÉ coloré : un anneau fin, moutarde, à quelques pixels du bord ;
//   4. le fond de l'assiette, un peu plus sombre au centre (le creux) : l'image en cubes y repose.
inline void DrawPlate(Dl& dl, float x0, float y0, float x1, float y1) {
    const float w = x1 - x0, h = y1 - y0, r = NkMin(w, h) * 0.22f;
    for (int i = 4; i >= 1; --i) {
        float g = i * 5.f;
        dl.AddRectFilled(nkgui::NkRect(x0 - g + 6.f, y0 - g + 12.f, w + 2 * g, h + 2 * g), NkColor(20, 50, 15, 22), r + g);
    }
    dl.AddRectFilled(nkgui::NkRect(x0, y0 + 4.f, w, h), NkColor(160, 150, 130, 255), r);                  // épaisseur de la porcelaine
    dl.AddRectFilled(nkgui::NkRect(x0, y0, w, h), NkColor(250, 246, 238, 255), r);                        // bord
    dl.AddRectFilled(nkgui::NkRect(x0 + 6.f, y0 + 4.f, w - 12.f, h * .45f), NkColor(255, 255, 255, 120), r * .8f);   // reflet
    dl.AddRect(nkgui::NkRect(x0 + 11.f, y0 + 11.f, w - 22.f, h - 22.f), pal::Moutarde(), 3.f, r - 10.f);  // liseré
    dl.AddRectFilled(nkgui::NkRect(x0 + 18.f, y0 + 18.f, w - 36.f, h - 36.f), NkColor(150, 178, 190, 255), r - 16.f);
    dl.AddRectFilled(nkgui::NkRect(x0 + 24.f, y0 + 26.f, w - 48.f, h - 52.f), NkColor(166, 192, 203, 255), r - 22.f);   // creux : teinte MOYENNE et douce où le blanc ET le noir se lisent
}

// ---- Panier d'osier --------------------------------------------------------------------------------------------
// Corps en trapèze (plus large en haut), tressage fait de TRAITS : trois cordons horizontaux et des montants qui
// suivent l'évasement, anse en courbe de Bézier, bord ovale, étiquette crème portant le nombre restant.
// La teinte du panier est celle des fourmis : osier mélangé à 42 % de la couleur du slot.
// `rebond` > 0 gonfle et soulève le panier (il vient de recevoir un bloc) ; `actif` faux dessine un panier vide pâle.
inline void DrawBasket(Dl& dl, float cx, float cy, float sz, const NkColor& tint, const char* nb, bool actif, float rebond) {
    const float w = sz, h = sz * .78f;
    if (!actif) {                                                                                   // emplacement libre : silhouette pâle
        NkVec2f p[6] = { NkVec2f(cx - .5f*w, cy - .5f*h), NkVec2f(cx + .5f*w, cy - .5f*h), NkVec2f(cx + .42f*w, cy + .25f*h), NkVec2f(cx + .30f*w, cy + .5f*h), NkVec2f(cx - .30f*w, cy + .5f*h), NkVec2f(cx - .42f*w, cy + .25f*h) };
        dl.AddConvexPolyFilled(p, 6, NkColor(255, 250, 235, 70));
        dl.AddPolylineLisse(p, 6, NkColor(255, 250, 235, 150), 2.f, true);
        return;
    }
    const float k = 1.f + 0.10f * rebond; cy -= 9.f * rebond;
    const float W = w * k, H = h * k;
    NkColor base = pal::Osier().Lerp(tint, 0.90f), dark = tint.Shade(0.58f), light = Lighten(tint, 0.28f);   // 90 % de la couleur des fourmis : tressage plus sombre de la MÊME teinte
    dl.AddEllipseFilled(NkVec2f(cx + 3.f, cy + H * .52f), W * .52f, H * .13f, NkColor(20, 50, 15, 55));    // ombre sur l'herbe
    dl.AddBezierCubic(NkVec2f(cx - .40f*W, cy - .46f*H), NkVec2f(cx - .55f*W, cy - 1.35f*H), NkVec2f(cx + .55f*W, cy - 1.35f*H), NkVec2f(cx + .40f*W, cy - .46f*H), dark, W * .075f, 0, true);   // anse
    NkVec2f p[6] = { NkVec2f(cx - .5f*W, cy - .46f*H), NkVec2f(cx + .5f*W, cy - .46f*H), NkVec2f(cx + .42f*W, cy + .25f*H), NkVec2f(cx + .30f*W, cy + .5f*H), NkVec2f(cx - .30f*W, cy + .5f*H), NkVec2f(cx - .42f*W, cy + .25f*H) };
    NkVec2f po[6]; for (int i = 0; i < 6; ++i) po[i] = NkVec2f(cx + (p[i].x - cx) * 1.05f, cy + (p[i].y - cy) * 1.05f + (i > 2 ? 1.f : 0.f));
    dl.AddConvexPolyFilled(po, 6, dark.Shade(0.7f));                                                // contour
    dl.AddConvexPolyFilled(p, 6, base);
    // Tressage. La largeur du corps à l'ordonnée y varie linéairement de W (haut) à ~0.6 W (bas) : on la recalcule
    // pour que les cordons et les montants suivent le bord.
    auto larg = [&](float y) { float t = (y - (cy - .46f * H)) / (H * .96f); return W * (1.0f - 0.40f * NkClamp(t, 0.f, 1.f)); };
    for (int j = 1; j <= 3; ++j) {                                                                  // cordons horizontaux
        float y = cy - .46f * H + H * .96f * (j / 4.f), lw = larg(y) * .5f;
        dl.AddLine(NkVec2f(cx - lw, y), NkVec2f(cx + lw, y), dark, 1.6f);
        dl.AddLine(NkVec2f(cx - lw, y - 1.4f), NkVec2f(cx + lw, y - 1.4f), light, 1.0f);
    }
    for (int j = -3; j <= 3; ++j) {                                                                 // montants verticaux (suivent l'évasement)
        float f = j / 3.5f * .5f;
        dl.AddLine(NkVec2f(cx + f * larg(cy - .46f * H), cy - .46f * H), NkVec2f(cx + f * larg(cy + .5f * H), cy + .5f * H), dark.Lerp(base, .45f), 1.2f);
    }
    dl.AddEllipseFilled(NkVec2f(cx, cy - .46f * H), W * .52f, H * .13f, dark);                      // bord ovale
    dl.AddEllipseFilled(NkVec2f(cx, cy - .46f * H + 1.f), W * .44f, H * .085f, dark.Shade(0.45f));  // intérieur sombre
    Pill(dl, cx, cy + .12f * H, W * .62f, H * .46f, pal::Brun(), H * .14f);                        // étiquette sombre : le nombre blanc contouré se lit sur toute teinte
    GameTextOutline(dl, cx, cy + .11f * H, H * .46f, nb, NkColor(255, 255, 255, 255), NkColor(40, 26, 18, 255));
}

// ---- Alvéole du rayon de miel ----------------------------------------------------------------------------------
// Une cellule hexagonale à sommets plats : liseré clair si elle est jouable, lèvre dessous (épaisseur), face de la couleur
// EXACTE du bloc, reflet sur l'arête haute, nombre contouré. `dispo` faux : la teinte est PÂLIE (mélangée de blanc), jamais
// brunie. `mystere` : fond gris-bleu sombre propre aux blocs mystère. `k` (0..1] fait pousser la cellule (apparition), `sx`
// l'écrase en largeur (bascule de la révélation d'un mystère).
inline void DrawHoneyCell(Dl& dl, float cx, float cy, float R, const NkColor& c, const char* txt, bool dispo, bool mystere, bool joue, float k = 1.f, float sx = 1.f) {
    NkColor base = mystere ? MysteryColor() : c;
    // DISPONIBLE : couleur pleine, plus marquée (bord brun, relief), relief (lèvre, face, reflet), bord brun marqué : même un blanc existe.
    // NON DISPONIBLE : teinte pâlie, plus petite (x 0.84), PLATE (une seule face, pas de lèvre ni de reflet). L'écart saute aux yeux.
    if (!dispo) base = base.Lerp(NkColor(246, 242, 232, 255), 0.55f);
    R *= k * (dispo ? 0.96f : 0.82f);
    const NkColor bord = pal::Brun();
    dl.AddEllipseFilled(NkVec2f(cx + R * .06f, cy + R * .86f), R * .90f * sx, R * .22f, NkColor(30, 20, 10, dispo ? 70 : 40));   // ombre portée (le blanc existe)
    if (dispo) {
        if (joue) HexPolyFlat(dl, cx, cy, R * 1.08f, NkColor(255, 255, 255, 240), sx);              // anneau de sélection
        HexPolyFlat(dl, cx, cy + R * .10f, R, bord.Shade(0.8f), sx);                                // lèvre sombre
        HexPolyFlat(dl, cx, cy, R * .98f, bord, sx);                                                // bord brun marqué
        HexPolyFlat(dl, cx, cy - R * .02f, R * .90f, base, sx);
        HexPolyFlat(dl, cx, cy - R * .08f, R * .66f, Lighten(base, mystere ? 0.10f : 0.22f), sx);
        const float h = R * .866f;
        dl.AddLine(NkVec2f(cx - R * .38f * sx, cy - h * .78f), NkVec2f(cx + R * .30f * sx, cy - h * .78f), NkColor(255, 255, 255, 190), R * .08f);
    } else {
        HexPolyFlat(dl, cx, cy, R * .98f, base.Shade(0.78f), sx);
        HexPolyFlat(dl, cx, cy, R * .90f, base, sx);
    }
    // Nombre : toujours blanc, GRAS (épaissi), contour sombre épais, quelle que soit la teinte.
    if (sx > 0.35f && k > 0.5f) GameTextOutline(dl, cx, cy - R * .02f, R * (dispo ? 0.86f : 0.70f), txt, NkColor(255, 255, 255, dispo ? 255 : 230), NkColor(40, 26, 18, dispo ? 255 : 200));
}
// Alvéole vide (place du rayon sans bloc) : un creux ambré à peine visible, pour que le rayon reste lisible.
inline void DrawEmptyCell(Dl& dl, float cx, float cy, float R) {
    HexPolyFlat(dl, cx, cy, R * .96f, NkColor(120, 84, 30, 60));
    HexPolyFlat(dl, cx, cy + R * .04f, R * .84f, NkColor(70, 44, 14, 45));
}

// ---- Icônes des bonus --------------------------------------------------------------------------------------------
// Chaque icône tient dans un carré de côté 2 r centré en (cx, cy). Elles se dessinent par formes pleines simples
// (disques, polygones, rectangles arrondis) avec un contour brun posé d'abord, un peu plus grand : à petite taille
// le contour fait la lecture, la couleur ne fait que l'habiller.
inline void IconPanier(Dl& dl, float cx, float cy, float r) {                                          // + un emplacement
    DrawBasket(dl, cx - r * .12f, cy + r * .12f, r * 1.25f, NkColor(255, 255, 255, 255), "", true, 0.f);
    dl.AddRectFilled(nkgui::NkRect(cx + r * .22f, cy - r * .72f, r * .66f, r * .22f), NkColor(255, 255, 255, 255), r * .05f);
    dl.AddRectFilled(nkgui::NkRect(cx + r * .44f, cy - r * .94f, r * .22f, r * .66f), NkColor(255, 255, 255, 255), r * .05f);
}
inline void IconMain(Dl& dl, float cx, float cy, float r) {                                            // choisir n'importe quel bloc : la main
    for (int pass = 0; pass < 2; ++pass) {
        float e = pass == 0 ? r * .10f : 0.f; NkColor c = pass == 0 ? pal::Brun() : NkColor(255, 238, 214, 255);
        for (int f = 0; f < 4; ++f) {                                                                 // quatre doigts de hauteurs inégales
            static const float hh[4] = {.62f, .78f, .72f, .52f};
            float fx = cx - r * .42f + f * r * .28f, w = r * .26f + 2 * e, h = r * hh[f] + 2 * e;
            dl.AddRectFilled(nkgui::NkRect(fx - w * .5f, cy - r * .16f - h + r * .10f, w, h), c, w * .5f);
        }
        dl.AddRectFilled(nkgui::NkRect(cx - r * .56f - e, cy - r * .20f - e, r * 1.12f + 2 * e, r * .92f + 2 * e), c, r * .28f);   // paume
        dl.AddEllipseFilled(NkVec2f(cx - r * .62f, cy + r * .10f), r * .24f + e, r * .15f + e, c);   // pouce
    }
}
inline void IconBombe(Dl& dl, float cx, float cy, float r) {                                           // le canon : une bombe à mèche
    dl.AddEllipseFilled(NkVec2f(cx - r * .08f, cy + r * .18f), r * .66f + r * .09f, r * .66f + r * .09f, pal::Brun());
    dl.AddEllipseFilled(NkVec2f(cx - r * .08f, cy + r * .18f), r * .66f, r * .66f, NkColor(58, 58, 74, 255));
    dl.AddEllipseFilled(NkVec2f(cx - r * .26f, cy - r * .02f), r * .20f, r * .14f, NkColor(190, 196, 220, 220));      // reflet
    dl.AddRectFilled(nkgui::NkRect(cx + r * .02f, cy - r * .68f, r * .30f, r * .22f), NkColor(120, 120, 140, 255), r * .05f);   // culot
    dl.AddBezierCubic(NkVec2f(cx + r * .22f, cy - r * .66f), NkVec2f(cx + r * .36f, cy - r * 1.0f), NkVec2f(cx + r * .56f, cy - r * .7f), NkVec2f(cx + r * .66f, cy - r * .90f), NkColor(120, 78, 50, 255), r * .09f, 0, true);   // mèche
    Star(dl, cx + r * .70f, cy - r * .94f, r * .26f, NkColor(255, 214, 70, 255));                       // étincelle
}
inline void IconEclair(Dl& dl, float cx, float cy, float r) {                                          // fourmis plus vite et plus nombreuses : l'éclair
    static const float X[6] = {.25f, -.50f, -.02f, -.25f, .50f, .00f}, Y[6] = {-1.f, .10f, .10f, 1.f, -.20f, -.20f};
    for (int pass = 0; pass < 2; ++pass) {
        float e = pass == 0 ? 1.28f : 1.f; NkColor c = pass == 0 ? pal::Brun() : NkColor(255, 232, 90, 255);
        NkVec2f P[6]; for (int i = 0; i < 6; ++i) P[i] = NkVec2f(cx + X[i] * r * e * .9f, cy + Y[i] * r * e * .9f);
        static const int T[4][3] = {{0, 1, 2}, {0, 2, 5}, {5, 2, 4}, {2, 3, 4}};
        for (int t = 0; t < 4; ++t) dl.AddTriangleFilled(P[T[t][0]], P[T[t][1]], P[T[t][2]], c);
    }
}

// ---- Confettis --------------------------------------------------------------------------------------------------
inline void DrawConfetti(Dl& dl, float x, float y, float rot, float w, const NkColor& c) {
    float ca = NkCos(rot), sa = NkSin(rot), hw = w * .5f, hh = w * .28f;
    NkVec2f q[4] = { NkVec2f(x - hw * ca + hh * sa, y - hw * sa - hh * ca), NkVec2f(x + hw * ca + hh * sa, y + hw * sa - hh * ca),
                     NkVec2f(x + hw * ca - hh * sa, y + hw * sa + hh * ca), NkVec2f(x - hw * ca - hh * sa, y - hw * sa + hh * ca) };
    dl.AddConvexPolyFilled(q, 4, c);
}

} // namespace nk25d
