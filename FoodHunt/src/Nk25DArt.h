// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DArt.h : L'habillage « pique-nique au jardin » de FoodHunt, dessiné ENTIÈREMENT par le code
// (NkGuiDrawList) : aucun fichier d'art, sauf les images des niveaux. Créé le 05/11/2026.
//
// Direction artistique (Rodolf : « conserver le style, donc l'idée et non les assets ») : une herbe tendre, une
// assiette de céramique claire qui porte l'image en cubes, une fourmilière de terre, des paniers d'osier teintés de
// la couleur des fourmis, un rayon de miel pour le stock, de petites icônes dessinées pour les bonus.
// Tout est exprimé en coordonnées LOGIQUES (voir Gameplay::SetView) : l'application met ensuite l'ensemble à
// l'échelle de l'écran en une fois. Les fonctions sont libres et sans état : elles peuvent se tester seules.
//
// RÔLE DE CE FICHIER : c'est la boîte à outils de dessin du jeu. Il ne contient aucune règle ni aucune donnée de partie.
// Articulation : Nk25D.h fournit les types de base (Dl = la liste de dessin, NkColor, NkVec2f, GameTextOutline...) ;
// Nk25DBoard.h utilise la palette de jeu (GameColor, NearestGameColor) pour colorer les cubes ; Nk25DGame.h appelle
// DrawGrass, DrawPlate, DrawGingham, DrawBasket, DrawHoneyCell et les icônes pour peindre l'écran et l'interface.
#pragma once
#include "Nk25D.h"

namespace nk25d {

// ---- Palette de l'interface (une seule source : changer ici change tout le jeu) --------------------------------
// Chaque fonction rend une couleur RGBA (rouge, vert, bleu, opacité), chaque canal de 0 à 255.
namespace pal {
    // Crème : le fond clair de la nappe, des cartes et des étiquettes.
    inline NkColor Creme()
    {
        return NkColor(255, 247, 228, 255);
    }

    // Brun : texte sur fond clair, contours.
    inline NkColor Brun()
    {
        return NkColor(98, 62, 42, 255);
    }

    // Tomate : un rouge vif pour les boutons et les accents de l'interface.
    inline NkColor Tomate()
    {
        return NkColor(226, 76, 60, 255);
    }

    // Moutarde : le jaune chaud (liseré de l'assiette, bonus).
    inline NkColor Moutarde()
    {
        return NkColor(244, 190, 62, 255);
    }

    // Feuille : le vert des boutons de réussite.
    inline NkColor Feuille()
    {
        return NkColor(98, 170, 72, 255);
    }

    // Ciel : le bleu des boutons neutres.
    inline NkColor Ciel()
    {
        return NkColor(98, 162, 222, 255);
    }

    // Osier : la teinte de base des paniers, avant mélange avec la couleur des fourmis.
    inline NkColor Osier()
    {
        return NkColor(214, 168, 96, 255);
    }
}

// ---- LA PALETTE DE JEU (Rodolf, 05/11 : « les couleurs ne vont pas ») --------------------------------------------
// Douze couleurs (crème et brun clair ajoutés le 05/11 pour les pâtes et les croûtes), vives et bien SÉPARÉES.
// Chaque couleur d'une image de niveau est ramenée à la plus proche de celles-ci (Board::LoadFromImage) : un niveau
// n'a donc jamais deux couleurs proches, et la MÊME couleur exacte sert au cube, à la fourmi, au panier et à
// l'alvéole. Le blanc et le brun sombre y sont pour les images claires et sombres.
constexpr int kNbGameColors = 12;

// La couleur de jeu d'indice i (0 à 11). Un indice hors bornes est ramené à la couleur la plus proche de la plage
// (le premier ou le dernier), pour qu'une donnée fautive ne fasse jamais lire hors du tableau.
//   i : l'indice.   Rend la couleur, toujours opaque.
inline NkColor GameColor(int i)
{
    // Les douze couleurs en RGB. Ordre : rouge, orange, jaune, vert, turquoise, bleu, violet, rose, puis
    // 8 blanc, 9 brun sombre (contour), 10 crème, 11 brun clair.
    static const unsigned char t[kNbGameColors][3] = {
        {228, 52, 52},
        {255, 146, 30},
        {250, 214, 40},
        {84, 190, 62},
        {28, 190, 180},
        {52, 112, 232},
        {146, 76, 204},
        {250, 122, 190},
        {245, 245, 240},
        {104, 66, 40},
        {252, 228, 176},
        {196, 138, 80}
    };
    i = i < 0 ? 0 : (i >= kNbGameColors ? kNbGameColors - 1 : i);
    return NkColor(t[i][0], t[i][1], t[i][2], 255);
}

// Couleur la plus proche (distance « redmean », approximation de la perception : le rouge pèse plus quand le pixel est rouge).
//   r, g, b : la couleur à approcher (0 à 255).
//   mask    : les couleurs autorisées, un bit par indice (bit i = couleur i) ; par défaut toutes.
// Rend l'indice de la couleur de jeu la plus proche parmi celles autorisées (0 si le masque est vide).
inline int NearestGameColor(int r, int g, int b, unsigned mask = 0xffffffffu)
{
    int best = 0;
    // Distance maximale d'un long de 32 bits : n'importe quelle vraie distance est plus petite.
    long bd = 0x7fffffffL;
    for (int i = 0; i < kNbGameColors; ++i)
    {
        // Couleur exclue par le masque.
        if (!(mask & (1u << i)))
        {
            continue;
        }
        NkColor c = GameColor(i);
        // rm : la moyenne des rouges ; dr, dg, db : les écarts par canal.
        long rm = (r + c.r) / 2;
        long dr = r - c.r;
        long dg = g - c.g;
        long db = b - c.b;
        // Formule « redmean » : les poids du rouge et du bleu varient avec la moyenne des rouges, le vert pèse toujours 4.
        long d = (2 + rm / 256) * dr * dr + 4 * dg * dg + (2 + (255 - rm) / 256) * db * db;
        if (d < bd)
        {
            bd = d;
            best = i;
        }
    }
    return best;
}

// La couleur « mystère » : un gris-bleu sombre qu'aucune couleur de la palette ne rappelle.
inline NkColor MysteryColor()
{
    return NkColor(58, 66, 92, 255);
}

// Éclaircir sans changer de teinte : on mélange vers le blanc (Shade > 1 écrête chaque canal et vire au jaune/blanc).
//   c : la couleur.   t : la part de blanc, de 0 (aucune) à 1 (blanc pur).
inline NkColor Lighten(const NkColor& c, float t)
{
    return c.Lerp(NkColor(255, 255, 255, 255), t);
}

// ---- Petits outils -------------------------------------------------------------------------------------------

// Convertit des degrés en radians.
inline float Rad(float deg)
{
    return deg * (NK_PI_F / 180.f);
}

// Hexagone régulier « pointe en haut » de rayon R (distance centre-sommet). Sommets aux angles -90, -30, 30, 90...
// Pavage en rayon de miel : largeur sqrt(3) R, rangées espacées de 1.5 R, rangées impaires décalées d'une demi-largeur.
//   dl : la liste de dessin.   cx, cy : le centre.   R : le rayon.   c : la couleur de remplissage.
inline void HexPoly(Dl& dl, float cx, float cy, float R, const NkColor& c)
{
    NkVec2f p[6];
    for (int k = 0; k < 6; ++k)
    {
        // Un sommet tous les 60 degrés, en partant du haut (-90 degrés).
        float a = Rad(60.f * k - 90.f);
        p[k] = NkVec2f(cx + R * NkCos(a), cy + R * NkSin(a));
    }
    // Remplissage à bord lissé (anticrénelage).
    dl.AddConvexPolyLisse(p, 6, c);
}

// Test de clic dans l'hexagone « pointe en haut » (repère centré) : |dx| <= (sqrt(3)/2) R et |dy| <= R - |dx| / sqrt(3).
// La seconde inégalité est l'équation des deux arêtes obliques (pente 1/sqrt(3)) ; c'est exact, pas un cercle inscrit.
//   x, y : le point testé.   cx, cy : le centre.   R : le rayon.   Rend vrai si le point est dans l'hexagone.
inline bool InHex(float x, float y, float cx, float cy, float R)
{
    float dx = NkAbs(x - cx);
    float dy = NkAbs(y - cy);
    // 0,8660254 = sqrt(3) / 2 ; 0,5773503 = 1 / sqrt(3).
    return dx <= 0.8660254f * R && dy <= R - dx * 0.5773503f;
}

// Hexagone « sommets plats » (nid d'abeilles par COLONNES) : sommets aux angles 0, 60, 120... ; largeur 2 R, hauteur sqrt(3) R.
// Colonnes espacées de 1.5 R ; une colonne sur deux décalée d'une demi-hauteur ; DANS une colonne les alvéoles sont
// l'une sous l'autre, donc une alvéole qui monte dans sa colonne monte tout droit. `sx` écrase en largeur (bascule).
//   dl : la liste de dessin.   cx, cy : le centre.   R : le rayon.   c : la couleur.
//   sx : facteur de largeur (1 = normal, 0 = écrasé en un trait).
inline void HexPolyFlat(Dl& dl, float cx, float cy, float R, const NkColor& c, float sx = 1.f)
{
    NkVec2f p[6];
    for (int k = 0; k < 6; ++k)
    {
        // Un sommet tous les 60 degrés, en partant de la droite (0 degré) ; seul x est écrasé par sx.
        float a = Rad(60.f * k);
        p[k] = NkVec2f(cx + sx * R * NkCos(a), cy + R * NkSin(a));
    }
    dl.AddConvexPolyLisse(p, 6, c);
}

// Test de clic, hexagone à sommets plats : |dy| <= (sqrt(3)/2) R et |dx| <= R - |dy| / sqrt(3) (arêtes obliques, pente sqrt(3)).
//   Mêmes paramètres que InHex.
inline bool InHexFlat(float x, float y, float cx, float cy, float R)
{
    float dx = NkAbs(x - cx);
    float dy = NkAbs(y - cy);
    return dy <= 0.8660254f * R && dx <= R - dy * 0.5773503f;
}

// Étoile à cinq branches (non convexe : dix triangles issus du centre).
//   dl : la liste de dessin.   cx, cy : le centre.   R : le rayon des pointes.   c : la couleur.
inline void Star(Dl& dl, float cx, float cy, float R, const NkColor& c)
{
    NkVec2f p[10];
    for (int k = 0; k < 10; ++k)
    {
        // Dix sommets, un tous les 36 degrés : les pairs sont les pointes (rayon R), les impairs les creux (46 % de R).
        float a = Rad(36.f * k - 90.f);
        float r = (k & 1) ? R * 0.46f : R;
        p[k] = NkVec2f(cx + r * NkCos(a), cy + r * NkSin(a));
    }
    // Un triangle par arête du contour : centre, sommet k, sommet suivant.
    for (int k = 0; k < 10; ++k)
    {
        dl.AddTriangleFilled(NkVec2f(cx, cy), p[k], p[(k + 1) % 10], c);
    }
}

// Rectangle arrondi centré avec ombre douce en dessous : le « bouton » de l'interface.
//   dl : la liste de dessin.   cx, cy : le centre.   w, h : largeur et hauteur.   c : la couleur du bouton.
//   rounding : le rayon des coins ; négatif = la moitié de la hauteur (bouton en pilule).
inline void Pill(Dl& dl, float cx, float cy, float w, float h, const NkColor& c, float rounding = -1.f)
{
    float r = rounding < 0 ? h * .5f : rounding;
    // 1. L'ombre : le même rectangle, décalé de 9 % de la hauteur vers le bas, à 62 % de la teinte.
    // Le rayon est borné à 49,9 % du petit côté pour que les coins ne se chevauchent jamais.
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f + h * .09f, w, h), c.Shade(0.62f), NkMin(r, NkMin(w, h) * .499f));
    // 2. Le corps du bouton, un peu moins haut (93 %) pour laisser voir l'épaisseur de l'ombre.
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h * .93f), c, NkMin(r, NkMin(w, h) * .499f));
    // 3. Le reflet : une bande plus claire (116 %) en haut, haute de 34 % du bouton mais au plus 16 unités.
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f + h * .08f, cy - h * .5f + h * .06f, w - h * .16f, NkMin(h * .34f, 16.f)), c.Shade(1.16f), NkMin(r, h * .17f));
}

// Carte (panneaux de victoire et d'échec, 05/11) : ombre douce centrée un peu plus bas (quatre rectangles de plus en plus
// grands et transparents, comme l'assiette), corps uni, simple filet clair en haut. Plus de reflet en demi-carte.
//   dl : la liste de dessin.   cx, cy : le centre.   w, h : taille.   c : la couleur du corps.
//   rounding : le rayon des coins.
inline void Card(Dl& dl, float cx, float cy, float w, float h, const NkColor& c, float rounding)
{
    // Ombre : du plus grand (g = 16) au plus petit (g = 4), chaque couche transparente (alpha 26) ; l'empilement
    // fait un dégradé. Le décalage de 10 vers le bas place l'ombre sous la carte.
    for (int i = 4; i >= 1; --i)
    {
        float g = i * 4.f;
        dl.AddRectFilled(nkgui::NkRect(cx - w * .5f - g, cy - h * .5f - g + 10.f, w + 2 * g, h + 2 * g), NkColor(20, 40, 15, 26), rounding + g);
    }
    // Le corps uni de la carte.
    dl.AddRectFilled(nkgui::NkRect(cx - w * .5f, cy - h * .5f, w, h), c, rounding);
    // Un filet clair (blanc, alpha 160, épaisseur 2) rentré d'un pixel, qui souligne le bord.
    dl.AddRect(nkgui::NkRect(cx - w * .5f + 1.f, cy - h * .5f + 1.f, w - 2.f, h - 2.f), NkColor(255, 255, 255, 160), 2.f, rounding);
}

// ---- Fond : l'herbe --------------------------------------------------------------------------------------------
// (x0, y0)-(x1, y1) est la zone VISIBLE entière en coordonnées logiques : le fond couvre l'écran quelle que soit sa forme.
// 1. dégradé vertical doux (vert clair en haut, plus profond en bas) ;
// 2. brins d'herbe, trèfles et petites fleurs placés par une suite pseudo-aléatoire de graine fixe : identiques à chaque
//    trame (rien ne scintille) et discrets (alpha bas) pour ne pas gêner la lecture du jeu ;
// 3. vignettage : quatre bandes dégradées vert foncé -> transparent sur les bords.
inline void DrawGrass(Dl& dl, float x0, float y0, float x1, float y1)
{
    const float w = x1 - x0;
    const float h = y1 - y0;
    // 1. Le dégradé : les deux premières couleurs sont celles du haut, les deux dernières celles du bas.
    dl.AddRectFilledMultiColor(
        nkgui::NkRect(x0, y0, w, h),
        NkColor(176, 218, 118, 255),
        NkColor(176, 218, 118, 255),
        NkColor(116, 176, 84, 255),
        NkColor(116, 176, 84, 255));
    // La graine fixe 20261105 (la date du jour de création) : le même décor à chaque trame et à chaque lancement.
    // ATTENTION : l'ordre des tirages ci-dessous (brins, trèfles, fleurs) fait partie du dessin ; le changer déplacerait tout.
    NkLcg32 r;
    r.Seed(20261105u);
    // 2a. Brins d'herbe : trois petits traits en éventail, 70 touffes.
    for (int i = 0; i < 70; ++i)
    {
        // Position au hasard dans la zone (un tirage de 0 à 999, ramené à 0..1).
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w;
        float y = y0 + (float)r.NextUInt32(1000) * 0.001f * h;
        for (int k = -1; k <= 1; ++k)
        {
            // Trait de la base (x + 3k, y) vers la pointe, inclinée vers l'extérieur ; les brins latéraux sont un peu plus hauts.
            dl.AddLine(NkVec2f(x + k * 3.f, y), NkVec2f(x + k * 5.5f, y - 7.f - (float)(k * k) * 1.5f), NkColor(92, 150, 64, 70), 1.4f);
        }
    }
    // 2b. Trèfles : trois feuilles rondes et une tige, 26 trèfles.
    for (int i = 0; i < 26; ++i)
    {
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w;
        float y = y0 + (float)r.NextUInt32(1000) * 0.001f * h;
        // s : la taille du trèfle, de 5 à 7,9 unités.
        float s = 5.f + (float)r.NextUInt32(30) * 0.1f;
        NkColor g(84, 148, 66, 95);
        // La tige part sous les feuilles et descend en biais.
        dl.AddLine(NkVec2f(x, y + s), NkVec2f(x + 1.5f, y + s * 2.6f), g, 1.3f);
        // Les trois feuilles : gauche, droite, et une en haut.
        dl.AddEllipseFilled(NkVec2f(x - s * .6f, y + s * .2f), s * .62f, s * .62f, g);
        dl.AddEllipseFilled(NkVec2f(x + s * .6f, y + s * .2f), s * .62f, s * .62f, g);
        dl.AddEllipseFilled(NkVec2f(x, y - s * .5f), s * .62f, s * .62f, g);
    }
    // 2c. Fleurs : cinq pétales autour d'un coeur, 16 fleurs.
    for (int i = 0; i < 16; ++i)
    {
        float x = x0 + (float)r.NextUInt32(1000) * 0.001f * w;
        float y = y0 + (float)r.NextUInt32(1000) * 0.001f * h;
        // s : le rayon d'un pétale, de 3,2 à 5,1 unités.
        float s = 3.2f + (float)r.NextUInt32(20) * 0.1f;
        // Trois couleurs de pétales au hasard : blanc, jaune, rose (alpha 170 : discrètes).
        int t = (int)r.NextUInt32(3);
        NkColor pc = t == 0 ? NkColor(255, 255, 255, 170) : (t == 1 ? NkColor(255, 220, 90, 170) : NkColor(255, 170, 190, 170));
        // Les cinq pétales, un tous les 72 degrés, à 1,25 rayon du centre.
        for (int k = 0; k < 5; ++k)
        {
            dl.AddEllipseFilled(NkVec2f(x + s * 1.25f * NkCos(Rad(72.f * k)), y + s * 1.25f * NkSin(Rad(72.f * k))), s, s, pc);
        }
        // Le coeur doré, plus opaque.
        dl.AddEllipseFilled(NkVec2f(x, y), s * .8f, s * .8f, NkColor(244, 176, 40, 200));
    }
    // 3. Vignettage : vert foncé semi-transparent (sombre) vers vert foncé invisible (nul).
    const NkColor sombre(30, 70, 30, 80);
    const NkColor nul(30, 70, 30, 0);
    // e : l'épaisseur des bandes, 20 % du plus petit côté.
    const float e = NkMin(w, h) * 0.20f;
    // Bande du haut : sombre au bord, nulle vers l'intérieur.
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y0, w, e), sombre, sombre, nul, nul);
    // Bande du bas.
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y1 - e, w, e), nul, nul, sombre, sombre);
    // Bande de gauche.
    dl.AddRectFilledMultiColor(nkgui::NkRect(x0, y0, e, h), sombre, nul, nul, sombre);
    // Bande de droite.
    dl.AddRectFilledMultiColor(nkgui::NkRect(x1 - e, y0, e, h), nul, sombre, sombre, nul);
}

// ---- Nappe à carreaux (le bas de l'écran, derrière les boutons de bonus) --------------------------------------
// Le vichy : un fond crème, des bandes verticales rouges semi-transparentes, puis des bandes horizontales de même
// couleur ; là où elles se croisent le rouge s'additionne (carreau plus sombre), c'est ce qui fait le damier.
//   (x0, y0)-(x1, y1) : le rectangle à couvrir, en coordonnées logiques.
inline void DrawGingham(Dl& dl, float x0, float y0, float x1, float y1)
{
    // t : la largeur d'une bande, en unités ; une bande rouge sur deux, d'où un pas de 2 t.
    const float t = 16.f;
    // Ombre portée vers le haut : la nappe est posée sur l'herbe, elle jette une ombre sur le bord haut.
    dl.AddRectFilled(nkgui::NkRect(x0, y0 - 5.f, x1 - x0, 7.f), NkColor(30, 60, 20, 45));
    // Le fond crème.
    dl.AddRectFilled(nkgui::NkRect(x0, y0, x1 - x0, y1 - y0), pal::Creme());
    // Rouge semi-transparent (alpha 120) : c'est l'addition des couches qui fait le carreau plus sombre.
    const NkColor rouge(226, 76, 60, 120);
    // Bandes verticales ; la dernière est rognée pour ne pas dépasser (NkMin).
    for (float x = x0; x < x1; x += 2 * t)
    {
        dl.AddRectFilled(nkgui::NkRect(x, y0, NkMin(t, x1 - x), y1 - y0), rouge);
    }
    // Bandes horizontales.
    for (float y = y0; y < y1; y += 2 * t)
    {
        dl.AddRectFilled(nkgui::NkRect(x0, y, x1 - x0, NkMin(t, y1 - y)), rouge);
    }
    // Liseré clair au bord haut.
    dl.AddRectFilled(nkgui::NkRect(x0, y0, x1 - x0, 3.f), NkColor(255, 255, 255, 170));
}

// ---- L'assiette ------------------------------------------------------------------------------------------------
// Rectangle à grands coins ronds (rayon = 22 % du petit côté), de l'extérieur vers l'intérieur :
//   1. ombre portée douce sur l'herbe : quatre rectangles de plus en plus grands, de plus en plus transparents
//      (l'empilement d'alphas bas imite un flou gaussien) et décalés vers le bas-droite (la lumière vient d'en haut à gauche) ;
//   2. le bord : porcelaine crème, avec un reflet clair sur la moitié haute ;
//   3. le LISERÉ coloré : un anneau fin, moutarde, à quelques pixels du bord ;
//   4. le fond de l'assiette, un peu plus sombre au centre (le creux) : l'image en cubes y repose.
//   (x0, y0)-(x1, y1) : le rectangle de l'assiette, en coordonnées logiques.
inline void DrawPlate(Dl& dl, float x0, float y0, float x1, float y1)
{
    const float w = x1 - x0;
    const float h = y1 - y0;
    // r : le rayon des coins, 22 % du plus petit côté.
    const float r = NkMin(w, h) * 0.22f;
    // 1. Ombre : de la plus large (g = 20) à la plus étroite (g = 5), décalée de 6 à droite et 12 vers le bas.
    for (int i = 4; i >= 1; --i)
    {
        float g = i * 5.f;
        dl.AddRectFilled(nkgui::NkRect(x0 - g + 6.f, y0 - g + 12.f, w + 2 * g, h + 2 * g), NkColor(20, 50, 15, 22), r + g);
    }
    // Épaisseur de la porcelaine : un rectangle gris chaud décalé de 4 vers le bas, vu comme la tranche de l'assiette.
    dl.AddRectFilled(nkgui::NkRect(x0, y0 + 4.f, w, h), NkColor(160, 150, 130, 255), r);
    // 2. Bord : la porcelaine crème.
    dl.AddRectFilled(nkgui::NkRect(x0, y0, w, h), NkColor(250, 246, 238, 255), r);
    // Reflet : un voile blanc sur les 45 % du haut.
    dl.AddRectFilled(nkgui::NkRect(x0 + 6.f, y0 + 4.f, w - 12.f, h * .45f), NkColor(255, 255, 255, 120), r * .8f);
    // 3. Liseré moutarde : un anneau de 3 unités d'épaisseur, à 11 unités du bord.
    dl.AddRect(nkgui::NkRect(x0 + 11.f, y0 + 11.f, w - 22.f, h - 22.f), pal::Moutarde(), 3.f, r - 10.f);
    // 4. Le fond de l'assiette, bleu-gris, à 18 unités du bord.
    dl.AddRectFilled(nkgui::NkRect(x0 + 18.f, y0 + 18.f, w - 36.f, h - 36.f), NkColor(150, 178, 190, 255), r - 16.f);
    // Creux : teinte MOYENNE et douce où le blanc ET le noir se lisent (les cubes blancs et les cubes sombres y restent visibles).
    dl.AddRectFilled(nkgui::NkRect(x0 + 24.f, y0 + 26.f, w - 48.f, h - 52.f), NkColor(166, 192, 203, 255), r - 22.f);
}

// ---- Panier d'osier --------------------------------------------------------------------------------------------
// Corps en trapèze (plus large en haut), tressage fait de TRAITS : trois cordons horizontaux et des montants qui
// suivent l'évasement, anse en courbe de Bézier, bord ovale, étiquette crème portant le nombre restant.
// La teinte du panier est celle des fourmis : osier mélangé à 90 % de la couleur du slot (voir `base` plus bas).
// `rebond` > 0 gonfle et soulève le panier (il vient de recevoir un bloc) ; `actif` faux dessine un panier vide pâle.
//   dl : la liste de dessin.   cx, cy : le centre.   sz : la largeur du panier.   tint : la couleur du panier.
//   nb : le texte du nombre restant.   actif : panier en service ou emplacement libre.
//   rebond : de 0 à 1, l'amplitude du rebond (gonflement de 10 % et soulèvement de 9 unités au maximum).
inline void DrawBasket(Dl& dl, float cx, float cy, float sz, const NkColor& tint, const char* nb, bool actif, float rebond)
{
    // w : largeur, h : hauteur (78 % de la largeur).
    const float w = sz;
    const float h = sz * .78f;
    // Emplacement libre : silhouette pâle (le contour du trapèze seulement, pas de tressage).
    if (!actif)
    {
        NkVec2f p[6] = {
            NkVec2f(cx - .5f*w, cy - .5f*h),
            NkVec2f(cx + .5f*w, cy - .5f*h),
            NkVec2f(cx + .42f*w, cy + .25f*h),
            NkVec2f(cx + .30f*w, cy + .5f*h),
            NkVec2f(cx - .30f*w, cy + .5f*h),
            NkVec2f(cx - .42f*w, cy + .25f*h)
        };
        dl.AddConvexPolyFilled(p, 6, NkColor(255, 250, 235, 70));
        dl.AddPolylineLisse(p, 6, NkColor(255, 250, 235, 150), 2.f, true);
        return;
    }
    // Le rebond gonfle le panier (k va de 1 à 1,10) et le soulève (jusqu'à 9 unités vers le haut).
    const float k = 1.f + 0.10f * rebond;
    cy -= 9.f * rebond;
    // W, H : la taille réelle du panier après gonflement.
    const float W = w * k;
    const float H = h * k;
    // base : 90 % de la couleur des fourmis, 10 % d'osier ; dark : tressage plus sombre de la MÊME teinte ;
    // light : reflets clairs du tressage.
    NkColor base = pal::Osier().Lerp(tint, 0.90f);
    NkColor dark = tint.Shade(0.58f);
    NkColor light = Lighten(tint, 0.28f);
    // Ombre sur l'herbe : une ellipse plate sous le panier, décalée de 3 vers la droite.
    dl.AddEllipseFilled(NkVec2f(cx + 3.f, cy + H * .52f), W * .52f, H * .13f, NkColor(20, 50, 15, 55));
    // L'anse : une courbe de Bézier cubique qui monte au-dessus du panier (points de contrôle à 1,35 H au-dessus).
    dl.AddBezierCubic(
        NkVec2f(cx - .40f*W, cy - .46f*H),
        NkVec2f(cx - .55f*W, cy - 1.35f*H),
        NkVec2f(cx + .55f*W, cy - 1.35f*H),
        NkVec2f(cx + .40f*W, cy - .46f*H),
        dark,
        W * .075f,
        0,
        true);
    // Le corps : un trapèze de six points (haut gauche, haut droit, flanc droit, bas droit, bas gauche, flanc gauche).
    NkVec2f p[6] = {
        NkVec2f(cx - .5f*W, cy - .46f*H),
        NkVec2f(cx + .5f*W, cy - .46f*H),
        NkVec2f(cx + .42f*W, cy + .25f*H),
        NkVec2f(cx + .30f*W, cy + .5f*H),
        NkVec2f(cx - .30f*W, cy + .5f*H),
        NkVec2f(cx - .42f*W, cy + .25f*H)
    };
    // Le contour : le même trapèze agrandi de 5 % autour du centre (et les trois points du bas descendus d'une unité).
    NkVec2f po[6];
    for (int i = 0; i < 6; ++i)
    {
        po[i] = NkVec2f(cx + (p[i].x - cx) * 1.05f, cy + (p[i].y - cy) * 1.05f + (i > 2 ? 1.f : 0.f));
    }
    // Contour : très sombre (dark à 70 %).
    dl.AddConvexPolyFilled(po, 6, dark.Shade(0.7f));
    dl.AddConvexPolyFilled(p, 6, base);
    // Tressage. La largeur du corps à l'ordonnée y varie linéairement de W (haut) à ~0.6 W (bas) : on la recalcule
    // pour que les cordons et les montants suivent le bord.
    auto larg = [&](float y)
    {
        // t : la position verticale dans le corps, de 0 (haut) à 1 (bas), bornée.
        float t = (y - (cy - .46f * H)) / (H * .96f);
        return W * (1.0f - 0.40f * NkClamp(t, 0.f, 1.f));
    };
    // Cordons horizontaux : trois, à 1/4, 2/4 et 3/4 de la hauteur du corps.
    for (int j = 1; j <= 3; ++j)
    {
        float y = cy - .46f * H + H * .96f * (j / 4.f);
        // lw : la demi-largeur du corps à cette hauteur.
        float lw = larg(y) * .5f;
        // Le cordon sombre, puis le liseré clair juste au-dessus (1,4 unité plus haut) : l'effet de relief.
        dl.AddLine(NkVec2f(cx - lw, y), NkVec2f(cx + lw, y), dark, 1.6f);
        dl.AddLine(NkVec2f(cx - lw, y - 1.4f), NkVec2f(cx + lw, y - 1.4f), light, 1.0f);
    }
    // Montants verticaux (suivent l'évasement) : sept traits, de -3 à +3.
    for (int j = -3; j <= 3; ++j)
    {
        // f : la position du montant en fraction de la largeur, de -0,43 à +0,43.
        float f = j / 3.5f * .5f;
        dl.AddLine(NkVec2f(cx + f * larg(cy - .46f * H), cy - .46f * H), NkVec2f(cx + f * larg(cy + .5f * H), cy + .5f * H), dark.Lerp(base, .45f), 1.2f);
    }
    // Bord ovale du dessus, puis son intérieur plus sombre (le creux du panier).
    dl.AddEllipseFilled(NkVec2f(cx, cy - .46f * H), W * .52f, H * .13f, dark);
    dl.AddEllipseFilled(NkVec2f(cx, cy - .46f * H + 1.f), W * .44f, H * .085f, dark.Shade(0.45f));
    // Étiquette sombre : le nombre blanc contouré se lit sur toute teinte.
    Pill(dl, cx, cy + .12f * H, W * .62f, H * .46f, pal::Brun(), H * .14f);
    // Le nombre, blanc avec un contour brun presque noir.
    GameTextOutline(dl, cx, cy + .11f * H, H * .46f, nb, NkColor(255, 255, 255, 255), NkColor(40, 26, 18, 255));
}

// ---- Alvéole du rayon de miel ----------------------------------------------------------------------------------
// Une cellule hexagonale à sommets plats : liseré clair si elle est jouable, lèvre dessous (épaisseur), face de la couleur
// EXACTE du bloc, reflet sur l'arête haute, nombre contouré. `dispo` faux : la teinte est PÂLIE (mélangée de blanc), jamais
// brunie. `mystere` : fond gris-bleu sombre propre aux blocs mystère. `k` (0..1] fait pousser la cellule (apparition), `sx`
// l'écrase en largeur (bascule de la révélation d'un mystère).
//   dl : la liste de dessin.   cx, cy : le centre.   R : le rayon.   c : la couleur du bloc.   txt : le nombre affiché.
//   dispo : la cellule est jouable.   mystere : bloc mystère.   joue : la cellule est sélectionnée (anneau blanc).
inline void DrawHoneyCell(Dl& dl, float cx, float cy, float R, const NkColor& c, const char* txt, bool dispo, bool mystere, bool joue, float k = 1.f, float sx = 1.f)
{
    NkColor base = mystere ? MysteryColor() : c;
    // DISPONIBLE : couleur pleine, plus marquée (bord brun, relief), relief (lèvre, face, reflet), bord brun marqué : même un blanc existe.
    // NON DISPONIBLE : même teinte assombrie, plus petite, PLATE (une seule face, pas de lèvre ni de reflet). L'écart saute aux yeux.
    if (!dispo)
    {
        // (05/11) assombrie et non pâlie : mêlé de crème, le rouge devenait rose, une AUTRE couleur du jeu.
        base = base.Shade(0.72f);
    }
    // Le rayon : multiplié par k (apparition progressive), puis par 0,96 si jouable ou 0,82 sinon (plus petite).
    R *= k * (dispo ? 0.96f : 0.82f);
    const NkColor bord = pal::Brun();
    // Ombre portée sous la cellule (le blanc existe : elle aide à le distinguer du fond).
    dl.AddEllipseFilled(NkVec2f(cx + R * .06f, cy + R * .86f), R * .90f * sx, R * .22f, NkColor(30, 20, 10, dispo ? 70 : 40));
    if (dispo)
    {
        if (joue)
        {
            // Anneau de sélection : un hexagone blanc un peu plus grand que la cellule.
            HexPolyFlat(dl, cx, cy, R * 1.08f, NkColor(255, 255, 255, 240), sx);
        }
        // Lèvre sombre : l'épaisseur de la cellule, décalée de 10 % vers le bas.
        HexPolyFlat(dl, cx, cy + R * .10f, R, bord.Shade(0.8f), sx);
        // Bord brun marqué.
        HexPolyFlat(dl, cx, cy, R * .98f, bord, sx);
        // La face, à la couleur exacte du bloc.
        HexPolyFlat(dl, cx, cy - R * .02f, R * .90f, base, sx);
        // Le dessus bombé : un hexagone plus petit, un peu plus clair (8 % de blanc).
        HexPolyFlat(dl, cx, cy - R * .08f, R * .66f, Lighten(base, 0.08f), sx);
        // Reflet : un trait blanc sur l'arête haute ; h est la demi-hauteur de l'hexagone (sqrt(3)/2 R = 0,866 R).
        const float h = R * .866f;
        dl.AddLine(NkVec2f(cx - R * .38f * sx, cy - h * .78f), NkVec2f(cx + R * .30f * sx, cy - h * .78f), NkColor(255, 255, 255, 190), R * .08f);
    }
    else
    {
        // Cellule plate : un bord un peu plus sombre (78 %) et la face, rien d'autre.
        HexPolyFlat(dl, cx, cy, R * .98f, base.Shade(0.78f), sx);
        HexPolyFlat(dl, cx, cy, R * .90f, base, sx);
    }
    // Nombre : toujours blanc, GRAS (épaissi), contour sombre épais, quelle que soit la teinte.
    // On ne l'écrit pas quand la cellule est presque écrasée (sx <= 0,35) ou à peine apparue (k <= 0,5) : il serait illisible.
    if (sx > 0.35f && k > 0.5f)
    {
        GameTextOutline(dl, cx, cy - R * .02f, R * (dispo ? 0.86f : 0.70f), txt, NkColor(255, 255, 255, dispo ? 255 : 230), NkColor(40, 26, 18, dispo ? 255 : 200));
    }
}

// Alvéole vide (place du rayon sans bloc) : un creux ambré à peine visible, pour que le rayon reste lisible.
//   dl : la liste de dessin.   cx, cy : le centre.   R : le rayon.
inline void DrawEmptyCell(Dl& dl, float cx, float cy, float R)
{
    HexPolyFlat(dl, cx, cy, R * .96f, NkColor(120, 84, 30, 60));
    // Un second hexagone plus sombre et décalé vers le bas : l'impression de profondeur.
    HexPolyFlat(dl, cx, cy + R * .04f, R * .84f, NkColor(70, 44, 14, 45));
}

// ---- Icônes des bonus --------------------------------------------------------------------------------------------
// Chaque icône tient dans un carré de côté 2 r centré en (cx, cy). Elles se dessinent par formes pleines simples
// (disques, polygones, rectangles arrondis) avec un contour brun posé d'abord, un peu plus grand : à petite taille
// le contour fait la lecture, la couleur ne fait que l'habiller.
//   Toutes les icônes : dl la liste de dessin, (cx, cy) le centre, r le demi-côté.

// Icône « plus un emplacement » : un petit panier blanc et un signe plus.
inline void IconPanier(Dl& dl, float cx, float cy, float r)
{
    DrawBasket(dl, cx - r * .12f, cy + r * .12f, r * 1.25f, NkColor(255, 255, 255, 255), "", true, 0.f);
    // Les deux barres du signe plus, en haut à droite.
    dl.AddRectFilled(nkgui::NkRect(cx + r * .22f, cy - r * .72f, r * .66f, r * .22f), NkColor(255, 255, 255, 255), r * .05f);
    dl.AddRectFilled(nkgui::NkRect(cx + r * .44f, cy - r * .94f, r * .22f, r * .66f), NkColor(255, 255, 255, 255), r * .05f);
}

// Icône « choisir n'importe quel bloc » : la main. Deux passes : d'abord le contour brun (formes agrandies de e),
// puis le remplissage clair par-dessus.
inline void IconMain(Dl& dl, float cx, float cy, float r)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        // Passe 0 : contour (agrandissement e de 10 % de r, brun) ; passe 1 : remplissage (e = 0, peau claire).
        float e = pass == 0 ? r * .10f : 0.f;
        NkColor c = pass == 0 ? pal::Brun() : NkColor(255, 238, 214, 255);
        // Quatre doigts de hauteurs inégales.
        for (int f = 0; f < 4; ++f)
        {
            // Hauteur relative de chaque doigt (le majeur est le plus haut).
            static const float hh[4] = {.62f, .78f, .72f, .52f};
            float fx = cx - r * .42f + f * r * .28f;
            float w = r * .26f + 2 * e;
            float h = r * hh[f] + 2 * e;
            dl.AddRectFilled(nkgui::NkRect(fx - w * .5f, cy - r * .16f - h + r * .10f, w, h), c, w * .5f);
        }
        // La paume.
        dl.AddRectFilled(nkgui::NkRect(cx - r * .56f - e, cy - r * .20f - e, r * 1.12f + 2 * e, r * .92f + 2 * e), c, r * .28f);
        // Le pouce.
        dl.AddEllipseFilled(NkVec2f(cx - r * .62f, cy + r * .10f), r * .24f + e, r * .15f + e, c);
    }
}

// Icône « le canon » : une bombe à mèche.
inline void IconBombe(Dl& dl, float cx, float cy, float r)
{
    // Le contour brun : un disque un peu plus grand (rayon + 0,09 r).
    dl.AddEllipseFilled(NkVec2f(cx - r * .08f, cy + r * .18f), r * .66f + r * .09f, r * .66f + r * .09f, pal::Brun());
    // Le corps : un disque gris sombre.
    dl.AddEllipseFilled(NkVec2f(cx - r * .08f, cy + r * .18f), r * .66f, r * .66f, NkColor(58, 58, 74, 255));
    // Reflet : une petite ellipse claire en haut à gauche.
    dl.AddEllipseFilled(NkVec2f(cx - r * .26f, cy - r * .02f), r * .20f, r * .14f, NkColor(190, 196, 220, 220));
    // Culot : le petit rectangle gris où la mèche est plantée.
    dl.AddRectFilled(nkgui::NkRect(cx + r * .02f, cy - r * .68f, r * .30f, r * .22f), NkColor(120, 120, 140, 255), r * .05f);
    // Mèche : une courbe de Bézier brune qui monte en zigzag.
    dl.AddBezierCubic(
        NkVec2f(cx + r * .22f, cy - r * .66f),
        NkVec2f(cx + r * .36f, cy - r * 1.0f),
        NkVec2f(cx + r * .56f, cy - r * .7f),
        NkVec2f(cx + r * .66f, cy - r * .90f),
        NkColor(120, 78, 50, 255),
        r * .09f,
        0,
        true);
    // Étincelle : une petite étoile jaune au bout de la mèche.
    Star(dl, cx + r * .70f, cy - r * .94f, r * .26f, NkColor(255, 214, 70, 255));
}

// Icône « fourmis plus vite et plus nombreuses » : l'éclair.
inline void IconEclair(Dl& dl, float cx, float cy, float r)
{
    // Les six sommets de l'éclair, en fractions de r (X de gauche à droite, Y de haut en bas).
    static const float X[6] = {.25f, -.50f, -.02f, -.25f, .50f, .00f};
    static const float Y[6] = {-1.f, .10f, .10f, 1.f, -.20f, -.20f};
    for (int pass = 0; pass < 2; ++pass)
    {
        // Passe 0 : contour brun, agrandi de 28 % ; passe 1 : l'éclair jaune.
        float e = pass == 0 ? 1.28f : 1.f;
        NkColor c = pass == 0 ? pal::Brun() : NkColor(255, 232, 90, 255);
        NkVec2f P[6];
        for (int i = 0; i < 6; ++i)
        {
            // 0,9 : l'éclair occupe 90 % du carré de l'icône, pour laisser de la place au contour.
            P[i] = NkVec2f(cx + X[i] * r * e * .9f, cy + Y[i] * r * e * .9f);
        }
        // Les quatre triangles qui recouvrent la forme non convexe (indices dans P).
        static const int T[4][3] = {{0, 1, 2}, {0, 2, 5}, {5, 2, 4}, {2, 3, 4}};
        for (int t = 0; t < 4; ++t)
        {
            dl.AddTriangleFilled(P[T[t][0]], P[T[t][1]], P[T[t][2]], c);
        }
    }
}

// ---- Confettis --------------------------------------------------------------------------------------------------

// Un confetti : un petit rectangle tourné autour de son centre.
//   dl : la liste de dessin.   x, y : le centre.   rot : l'angle en radians.   w : la longueur.   c : la couleur.
inline void DrawConfetti(Dl& dl, float x, float y, float rot, float w, const NkColor& c)
{
    // ca, sa : cosinus et sinus de l'angle ; hw : demi-longueur ; hh : demi-épaisseur (28 % de la longueur).
    float ca = NkCos(rot);
    float sa = NkSin(rot);
    float hw = w * .5f;
    float hh = w * .28f;
    // Les quatre coins : le rectangle (±hw, ±hh) tourné de rot (rotation 2D classique).
    NkVec2f q[4] = {
        NkVec2f(x - hw * ca + hh * sa, y - hw * sa - hh * ca),
        NkVec2f(x + hw * ca + hh * sa, y + hw * sa - hh * ca),
        NkVec2f(x + hw * ca - hh * sa, y + hw * sa + hh * ca),
        NkVec2f(x - hw * ca - hh * sa, y - hw * sa + hh * ca)
    };
    dl.AddConvexPolyFilled(q, 4, c);
}

} // namespace nk25d
