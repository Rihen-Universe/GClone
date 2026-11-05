// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25DBoard.h : Plateau de jeu : image -> blocs de pixels 2.5D, palette, fourmis par couleur.
// Migré le 04/11/2026 : l'image arrive en NkImage (NKImage), la réduction par NK_AREA, la palette par k-means
// (NkImagePalette::Quantize), les chemins par NkGridPath (NKNavigation). Aucune bibliothèque standard.
//
// RÔLE DE CE FICHIER : il contient les trois objets du « décor vivant » de la partie.
//   - Board     : la grille de cubes colorés (le plateau). Elle sait dire quels cubes sont prenables, chercher un
//                 chemin jusqu'à un cube, repousser une fourmi qui le heurte, et se dessiner (ombres, cubes) ;
//   - Hole      : le nid (la fourmilière) où les fourmis rapportent les cubes ;
//   - PixelGame : une petite logique d'équipe de fourmis (une fourmi ne prend que les cubes de SA couleur).
// Articulation : Nk25D.h définit les briques générales (Entity, Scene, Ant, Box, RenderQueue, Dl) que ce fichier
// utilise ; Nk25DArt.h fournit la palette de jeu (GameColor, NearestGameColor) ; Nk25DLevels.h appelle
// Board::LoadFromGrid ; Nk25DGame.h pilote tout cela pendant la partie.
#pragma once
#include "Nk25D.h"
#include "Nk25DArt.h"
#include "NKImage/Core/NkImage.h"
#include "NKImage/Core/NkImagePalette.h"
#include "NKNavigation/NkGridPath.h"

namespace nk25d {

// Nombre maximal de couleurs différentes sur un plateau (taille de la palette d'un niveau).
constexpr int MAX_PAL = 16;

// Valeur d'une case vide dans la grille (255 ne peut pas être un indice de palette, qui va de 0 à 15).
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
    // Taille de la grille, en cases (largeur gw, hauteur gh).
    int gw = 0;
    int gh = 0;

    // Taille d'une cellule (cs : côté du carré au sol) et hauteur d'un cube (ch), en unités de plan (le plan logique de
    // 540 x 1000 ; Gameplay::SetWindow les recalcule pour que le plateau tienne dans l'écran).
    float cs = 11;
    float ch = 8;

    // Coin haut gauche du plateau dans le monde, en unités de plan.
    float ox = 0;
    float oy = 0;

    // La palette du niveau : la couleur de chaque indice, et le nombre de couleurs utilisées (np).
    NkColor palette[MAX_PAL];
    int np = 0;

    // Constructeur : marque l'entité comme plateau (le jeu retrouve ainsi le plateau dans la scène).
    Board()
    {
        kind = KIND_BOARD;
    }

    // ---- Image -> grille ------------------------------------------------------
    // gridW : nombre de blocs en largeur ; maxColors : taille de palette (k-means) ;
    // autoBg : retire le fond (couleur du pourtour, relié aux bords). Alpha < 128 = vide.
    // 1. Réduction : NkImage::Resize(NK_AREA) fait la moyenne de chaque cellule pondérée par l'alpha.
    // 2. Fond (facultatif) : NkImagePalette::EstimateBg puis RemoveBackground.
    // 3. Quantification : k-means sur les couleurs des cellules opaques (NkImagePalette::Quantize).
    // Cette voie « image » n'est plus utilisée par le jeu (les niveaux sont des grilles embarquées, voir LoadFromGrid) ;
    // elle reste pour les essais et pour l'outil qui fabrique les grilles.
    //   src : l'image source.   Rend true si le plateau est rempli, false si l'image est invalide ou vide.
    bool LoadFromImage(const NkImage& src, int gridW, int maxColors, bool autoBg = false)
    {
        if (!src.IsValid() || gridW <= 0)
        {
            return false;
        }
        gw = gridW;
        // La hauteur suit le rapport de l'image (arrondi au plus proche), avec au moins une case.
        gh = NkMax(1, (int)(src.Height() * (float)gridW / src.Width() + 0.5f));
        // On travaille en RGBA 8 bits quel que soit le format de départ, pour lire les pixels de la même façon.
        NkImage rgba = (src.Format() == NkImagePixelFormat::NK_RGBA32) ? src.Copy() : src.Convert(NkImagePixelFormat::NK_RGBA32);
        if (!rgba.IsValid())
        {
            return false;
        }
        // Une cellule de grille = un pixel de l'image réduite.
        NkImage cells = rgba.Resize(gw, gh, NkResizeFilter::NK_AREA);
        if (!cells.IsValid())
        {
            return false;
        }
        if (autoBg)
        {
            // Seuil de 30 : tolérance de couleur pour reconnaître le fond.
            NkImagePalette::RemoveBackground(cells, NkImagePalette::EstimateBg(cells), 30);
        }

        // PALETTE DE JEU (05/11/2026). Avant : k-means (NkImagePalette::Quantize) donnait des teintes quelconques, parfois deux
        // couleurs presque identiques, jamais les mêmes d'un écran à l'autre. Maintenant chaque cellule opaque est ramenée à la
        // plus proche des dix couleurs FIXES et bien séparées de pal::GameColor ; on ne garde que les `maxColors` plus
        // fréquentes (les autres cellules passent à la plus proche des gardées) ; les doublons sont fusionnés d'office puisque
        // deux cellules ramenées à la même couleur de jeu ont le même indice. L'image reste reconnaissable, aucun niveau n'a
        // deux couleurs proches. Le compte par couleur (left_) se refait ensuite (Recount) : il suit la NOUVELLE palette.
        const int n = gw * gh;
        c_.Resize(n);
        cool_.Resize(n);
        acc_.Clear();
        // Toutes les cases commencent vides et sans pause.
        for (int i = 0; i < n; ++i)
        {
            c_[i] = EMPTY;
            cool_[i] = 0;
        }
        np = 0;
        // g : la couleur de jeu de chaque cellule (EMPTY si transparente) ; cnt : combien de cellules par couleur de jeu.
        NkVector<uint8> g;
        g.Resize((usize)n);
        int cnt[kNbGameColors] = {};
        for (int i = 0; i < n; ++i)
        {
            // Quatre octets par pixel (R, V, B, alpha) : px[3] est l'alpha.
            const uint8* px = cells.RowPtr(i / gw) + (i % gw) * 4;
            if (px[3] < 128)
            {
                // Case vide : presque transparente.
                g[i] = EMPTY;
                continue;
            }
            g[i] = (uint8)NearestGameColor(px[0], px[1], px[2]);
            ++cnt[g[i]];
        }
        // keep : masque de bits des couleurs gardées ; order : ces couleurs, de la plus fréquente à la moins fréquente.
        unsigned keep = 0;
        int order[kNbGameColors];
        int nk = 0;
        for (int k = 0; k < NkClamp(maxColors, 1, (int)kNbGameColors); ++k)
        {
            // On cherche la couleur non encore gardée la plus fréquente.
            int b = -1;
            for (int c = 0; c < kNbGameColors; ++c)
            {
                if (!(keep & (1u << c)) && cnt[c] > 0 && (b < 0 || cnt[c] > cnt[b]))
                {
                    b = c;
                }
            }
            // Plus aucune couleur à garder : on s'arrête avant d'avoir atteint maxColors.
            if (b < 0)
            {
                break;
            }
            keep |= 1u << b;
            order[nk++] = b;
        }
        // remap : couleur de jeu -> indice dans la palette du niveau (-1 = pas encore attribué).
        int remap[kNbGameColors];
        for (int c = 0; c < kNbGameColors; ++c)
        {
            remap[c] = -1;
        }
        // Les couleurs gardées entrent dans la palette dans l'ordre de fréquence.
        for (int k = 0; k < nk; ++k)
        {
            remap[order[k]] = np;
            palette[np++] = GameColor(order[k]);
        }
        // Les couleurs écartées passent à la plus proche des couleurs gardées.
        for (int c = 0; c < kNbGameColors; ++c)
        {
            if (remap[c] < 0 && cnt[c] > 0)
            {
                NkColor q = GameColor(c);
                remap[c] = remap[NearestGameColor(q.r, q.g, q.b, keep)];
            }
        }
        // On écrit la grille finale : chaque case opaque reçoit son indice de palette.
        for (int i = 0; i < n; ++i)
        {
            if (g[i] != EMPTY)
            {
                c_[i] = (uint8)remap[g[i]];
            }
        }
        time_ = 0;
        Recount();
        UpdateReach();
        return np > 0;
    }

    // ---- Grille EMBARQUÉE (05/11/2026) : les cases sont déjà faites, à leur taille exacte et dans les couleurs de la palette de
    // jeu (tools/embarquer_niveaux.py). Aucun rééchantillonnage, aucune quantification : on copie.
    // cases[y * w + x] = 255 (vide) ou l'indice dans `pal` ; pal[k] = indice dans GameColor.
    //   w, h    : taille de la grille en cases.
    //   cases   : w * h octets (255 = vide, sinon indice dans la palette du niveau).
    //   nPal    : nombre de couleurs du niveau (1 à MAX_PAL).
    //   pal     : pour chaque couleur du niveau, son indice dans la palette fixe de jeu (GameColor).
    // Rend true si la grille est chargée, false si les dimensions ou la palette sont invalides.
    bool LoadFromGrid(int w, int h, const unsigned char* cases, int nPal, const unsigned char* pal)
    {
        if (w <= 0 || h <= 0 || nPal <= 0 || nPal > MAX_PAL)
        {
            return false;
        }
        gw = w;
        gh = h;
        const int n = w * h;
        c_.Resize(n);
        cool_.Resize(n);
        acc_.Clear();
        np = nPal;
        // Chaque couleur du niveau reçoit sa vraie couleur dans la palette de jeu.
        for (int k = 0; k < nPal; ++k)
        {
            palette[k] = GameColor(pal[k]);
        }
        // On recopie les cases ; le 255 de la donnée et EMPTY ont la même valeur, la conversion est explicite pour le lecteur.
        for (int i = 0; i < n; ++i)
        {
            c_[i] = cases[i] == 255 ? EMPTY : (uint8)cases[i];
            cool_[i] = 0;
        }
        time_ = 0;
        Recount();
        UpdateReach();
        return true;
    }

    // ---- Requêtes ---------------------------------------------------------------

    // Nombre total de cubes qui restent sur le plateau, toutes couleurs confondues.
    int Remaining() const
    {
        int t = 0;
        for (int i = 0; i < np; ++i)
        {
            t += left_[i];
        }
        return t;
    }

    // Nombre de cubes restants d'une couleur (indice de palette) ; 0 si l'indice est hors palette.
    int Remaining(int color) const
    {
        return (color >= 0 && color < np) ? left_[color] : 0;
    }

    // Contenu de la case (cx, cy) : l'indice de couleur, ou EMPTY si la case est vide ou hors de la grille
    // (au-delà du bord, c'est du vide : cela simplifie tous les tests de voisinage).
    int At(int cx, int cy) const
    {
        return (cx < 0 || cy < 0 || cx >= gw || cy >= gh) ? EMPTY : c_[cy * gw + cx];
    }

    // Abscisse du centre de la case i (indice linéaire y * gw + x), en unités de plan : coin + (colonne + 0,5) cases.
    float CellX(int i) const
    {
        return ox + (i % gw + .5f) * cs;
    }

    // Ordonnée du centre de la case i, en unités de plan.
    float CellY(int i) const
    {
        return oy + (i / gw + .5f) * cs;
    }

    // Vrai si la case i porte un cube dont au moins un des 4 voisins est vide (ou hors de la grille).
    // Attention : « exposé » ne veut pas dire « accessible » (une poche fermée est exposée mais inatteignable).
    bool Exposed(int i) const
    {
        int cx = i % gw;
        int cy = i / gw;
        return c_[i] != EMPTY
            && (At(cx - 1, cy) == EMPTY
                || At(cx + 1, cy) == EMPTY
                || At(cx, cy - 1) == EMPTY
                || At(cx, cy + 1) == EMPTY);
    }

    // Bloc exposé de la couleur donnée le plus proche de (fx,fy), hors cellules en pause ; -1 si aucun
    //   color  : l'indice de couleur cherché.   fx, fy : le point de départ (monde, par exemple la position de la fourmi).
    // Rend l'indice de la case trouvée, ou -1. La distance est comparée au carré : pas besoin de racine pour trier.
    int FindNearest(int color, float fx, float fy) const
    {
        int best = -1;
        // Plus grande distance imaginable : n'importe quelle case réelle sera plus proche.
        float bd = 1e30f;
        for (int i = 0; i < gw * gh; ++i)
        {
            // On saute les cases d'une autre couleur, celles en pause (cool_ > time_), et celles inatteignables.
            if (c_[i] != color || cool_[i] > time_ || !Accessible(i))
            {
                continue;
            }
            float dx = CellX(i) - fx;
            float dy = CellY(i) - fy;
            float d = dx * dx + dy * dy;
            if (d < bd)
            {
                bd = d;
                best = i;
            }
        }
        return best;
    }

    // Nombre de blocs exposés par couleur (out[MAX_PAL])
    //   out : un tableau de MAX_PAL entiers, entièrement réécrit (0 pour les couleurs absentes).
    void ExposedCounts(int* out) const
    {
        for (int i = 0; i < MAX_PAL; ++i)
        {
            out[i] = 0;
        }
        for (int i = 0; i < gw * gh; ++i)
        {
            if (c_[i] != EMPTY && Accessible(i))
            {
                ++out[c_[i]];
            }
        }
    }

    // ---- Accessibilité : un bloc est ciblable si l'une de ses 4 voisines est une cellule vide reliée à l'extérieur
    // (4-connexité à travers les cellules vides). Les poches fermées (yeux retirés...) ne comptent pas.
    // L'accessibilité est TOUJOURS À JOUR quand on la lit (règle de Rodolf du 05/11/2026 : « pas de valeur vieille de
    // 0,2 s ») : retirer un bloc lève le drapeau `dirty_`, et la première lecture suivante refait le remplissage.
    // On ne le refait donc qu'une fois par changement, jamais à chaque lecture.

    // Refait le calcul d'accessibilité si un bloc a été retiré depuis le dernier calcul.
    // const_cast : la fonction est const pour les lecteurs, mais le cache d'accessibilité est un détail interne.
    void EnsureReach() const
    {
        if (dirty_)
        {
            const_cast<Board*>(this)->UpdateReach();
        }
    }

    // Vrai si le cube de la case i peut être atteint par une fourmi (voir plus haut).
    // Si le cache n'existe pas encore (acc_ vide), on se rabat sur la simple exposition.
    bool Accessible(int i) const
    {
        EnsureReach();
        return acc_.Size() ? acc_[i] != 0 : Exposed(i);
    }

    // Recalcule l'accessibilité de toutes les cases par un remplissage depuis le bord (pile explicite, pas de récursion).
    //   Étape 1 : toute case vide au bord de la grille est « reliée à l'extérieur » ;
    //   Étape 2 : on propage de proche en proche à travers les cases vides (4 voisines) ;
    //   Étape 3 : un cube est accessible s'il touche le bord de la grille ou une case vide reliée à l'extérieur.
    void UpdateReach()
    {
        dirty_ = false;
        int n = gw * gh;
        if (!n)
        {
            return;
        }
        acc_.Resize(n);
        // reach : 1 si la case vide est reliée à l'extérieur ; st : la pile du remplissage.
        NkVector<uint8> reach;
        reach.Resize(n);
        NkVector<int> st;
        for (int i = 0; i < n; ++i)
        {
            reach[i] = 0;
        }
        // Étape 1 : les cases vides du bord sont les graines du remplissage.
        for (int i = 0; i < n; ++i)
        {
            int x = i % gw;
            int y = i / gw;
            if (c_[i] == EMPTY && (x == 0 || y == 0 || x == gw - 1 || y == gh - 1))
            {
                reach[i] = 1;
                st.PushBack(i);
            }
        }
        // Les 4 voisines : gauche, droite, haut, bas.
        const int dx[4] = {-1, 1, 0, 0};
        const int dy[4] = {0, 0, -1, 1};
        // Étape 2 : on dépile une case, et on empile ses voisines vides pas encore visitées.
        while (st.Size() > 0)
        {
            int i = st[st.Size() - 1];
            st.PopBack();
            int x = i % gw;
            int y = i / gw;
            for (int k = 0; k < 4; ++k)
            {
                int nx = x + dx[k];
                int ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh)
                {
                    continue;
                }
                int j = ny * gw + nx;
                if (c_[j] == EMPTY && !reach[j])
                {
                    reach[j] = 1;
                    st.PushBack(j);
                }
            }
        }
        // Étape 3 : un cube est accessible s'il a une voisine hors de la grille (bord) ou une voisine vide reliée à l'extérieur.
        for (int i = 0; i < n; ++i)
        {
            acc_[i] = 0;
            if (c_[i] == EMPTY)
            {
                continue;
            }
            int x = i % gw;
            int y = i / gw;
            for (int k = 0; k < 4 && !acc_[i]; ++k)
            {
                int nx = x + dx[k];
                int ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh)
                {
                    acc_[i] = 1;
                }
                else if (reach[ny * gw + nx])
                {
                    acc_[i] = 1;
                }
            }
        }
    }

    // ---- Chemin (points de passage monde) jusqu'à une cellule d'accès du bloc `target`, ou jusqu'à un point.
    //   sx, sy : départ (monde).   target : la case du bloc visé.   out : tableau des points de passage (monde).
    //   maxN : taille de out.   Rend le nombre de points écrits (0 = aucun chemin).
    int FindPathToCell(float sx, float sy, int target, NkVec2f* out, int maxN) const
    {
        return Search(sx, sy, target, 0, 0, out, maxN);
    }

    // Même chose, mais vers un point libre (tx, ty) du monde, par exemple le nid.
    int FindPathToPoint(float sx, float sy, float tx, float ty, NkVec2f* out, int maxN) const
    {
        return Search(sx, sy, -1, tx, ty, out, maxN);
    }

    // Vrai si la case i contient encore un cube de la couleur donnée (la case a pu être prise entre-temps).
    bool Present(int i, int color) const
    {
        return i >= 0 && i < gw * gh && c_[i] == color;
    }

    // Met la case i « en pause » pendant sec secondes de jeu : FindNearest l'ignore jusque-là. Sert à ne pas
    // renvoyer une fourmi vers un cube qu'elle n'arrive pas à atteindre.
    void Cooldown(int i, float sec)
    {
        if (i >= 0 && i < gw * gh)
        {
            cool_[i] = time_ + sec;
        }
    }

    // Retire le cube de la case i. Rend sa couleur, ou EMPTY s'il n'y avait rien. Lève `dirty_` : l'accessibilité des
    // voisins a changé et sera recalculée à la prochaine lecture.
    int Take(int i)
    {
        int c = c_[i];
        if (c != EMPTY)
        {
            c_[i] = EMPTY;
            --left_[c];
            dirty_ = true;
        }
        return c;
    }

    // Le point du carré de la case i le plus proche de (px, py) : on borne le point dans le carré.
    //   qx, qy : sorties, le point trouvé (monde).
    void ClosestOnCell(int i, float px, float py, float& qx, float& qy) const
    {
        // Coin haut gauche du carré de la case (CellX et CellY donnent le centre, d'où le retrait d'une demi-case).
        float x0 = CellX(i) - cs * .5f;
        float y0 = CellY(i) - cs * .5f;
        qx = NkClamp(px, x0, x0 + cs);
        qy = NkClamp(py, y0, y0 + cs);
    }

    // ---- Entity ---------------------------------------------------------------

    // Fait avancer l'horloge du plateau (elle sert aux pauses de cases, voir Cooldown). dt en secondes de jeu.
    void Update(float dt) override
    {
        time_ += dt;
    }

    // Collision cercle / cases pleines : pour chaque case voisine, on cherche le point de la case le plus proche
    // du centre (borne du centre dans le carré) ; si la distance est inférieure au rayon, on repousse le cercle
    // le long de la normale (centre - point) de la profondeur de pénétration (r - d).
    // COLLISION FOURMI / CUBES. Une fourmi de rayon 8 * cs / 11 faisait 1,45 case de large : elle ne tenait pas dans un couloir
    // d'une case, se coinçait contre les coins, et le banc mesurait des blocages définitifs (d'où un drapeau resté un temps à
    // faux, les fourmis marchant alors sur les cubes). À cs / 17 (Gameplay::Init) elle fait 0,94 case et passe : collision
    // active, 25 niveaux sur 25 au banc. Si on regrossit les fourmis, repasser le banc.
    bool collideAnts = true;

    // Repousse un cercle (une fourmi) hors des cubes.
    //   cx, cy : centre du cercle (entrée et sortie : il est déplacé).
    //   r      : rayon du cercle.
    //   nx, ny : sortie, la dernière normale de poussée (pour que la fourmi se réoriente).
    // Rend true si le cercle a heurté au moins un cube.
    bool ResolveCircle(float& cx, float& cy, float r, float& nx, float& ny) override
    {
        if (!collideAnts)
        {
            return false;
        }
        // Rayon de collision réduit : passe dans les rainures d'1 cellule.
        r = NkMin(r, cs * 0.42f);
        bool hit = false;
        // Les cases à tester : celles que le cercle touche, plus une case de marge de chaque côté.
        int ix0 = (int)((cx - r - ox) / cs) - 1;
        int ix1 = (int)((cx + r - ox) / cs) + 1;
        int iy0 = (int)((cy - r - oy) / cs) - 1;
        int iy1 = (int)((cy + r - oy) / cs) + 1;
        // Deux passes : repousser un cube peut pousser le cercle contre un autre ; la seconde passe corrige.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int y = NkMax(iy0, 0); y <= iy1 && y < gh; ++y)
            {
                for (int x = NkMax(ix0, 0); x <= ix1 && x < gw; ++x)
                {
                    if (c_[y * gw + x] == EMPTY)
                    {
                        continue;
                    }
                    float qx;
                    float qy;
                    ClosestOnCell(y * gw + x, cx, cy, qx, qy);
                    // (dx, dy) : du point le plus proche vers le centre ; d : distance entre les deux.
                    float dx = cx - qx;
                    float dy = cy - qy;
                    float d = NkVec2f(dx, dy).Len();
                    // Le cercle ne touche pas ce cube.
                    if (d >= r)
                    {
                        continue;
                    }
                    if (d > 1e-4f)
                    {
                        // Cas normal : la normale va du cube vers le centre du cercle.
                        nx = dx / d;
                        ny = dy / d;
                    }
                    else
                    {
                        // Le centre est dans le cube (d presque nul) : on pousse depuis le centre de la case.
                        float ex = cx - CellX(y * gw + x);
                        float ey = cy - CellY(y * gw + x);
                        float l = NkMax(1e-4f, NkVec2f(ex, ey).Len());
                        nx = ex / l;
                        ny = ey / l;
                        d = 0;
                    }
                    // On repousse de la profondeur de pénétration (r - d) le long de la normale.
                    cx += nx * (r - d);
                    cy += ny * (r - d);
                    hit = true;
                }
            }
        }
        return hit;
    }

    // Ombre portée des cubes de bord, puis ombre de contact. Dessinée AVANT les cubes (voir la file de rendu).
    //   dl : la liste de dessin.   cam : la vue (projection et lumière).
    void DrawShadow(Dl& dl, const NkView25D& cam) const override
    {
        if (c_.Size() == 0)
        {
            return;
        }
        for (int cy = 0; cy < gh; ++cy)
        {
            for (int cx = 0; cx < gw; ++cx)
            {
                if (c_[cy * gw + cx] == EMPTY)
                {
                    continue;
                }
                // Seuls les cubes dont la droite ou le bas est libre projettent une ombre visible sur le sol.
                if (At(cx + 1, cy) != EMPTY && At(cx, cy + 1) != EMPTY)
                {
                    continue;
                }
                float x0 = ox + cx * cs;
                float y0 = oy + cy * cs;
                // L'ombre du dessus du cube (à la hauteur ch) projetée sur le sol : un quadrilatère.
                NkVec2f q[4] = {
                    cam.ShadowPos(x0, y0, ch),
                    cam.ShadowPos(x0 + cs, y0, ch),
                    cam.ShadowPos(x0 + cs, y0 + cs, ch),
                    cam.ShadowPos(x0, y0 + cs, ch)
                };
                // Noir très transparent (alpha 45 sur 255) : l'ombre s'ajoute sans cacher l'herbe.
                dl.AddConvexPolyFilled(q, 4, NkColor(0, 0, 0, 45));
            }
        }
        // Ombre de CONTACT : sous chaque arête avant exposée, une fine bande sombre sur l'assiette (là où le cube
        // touche le plateau, la lumière ne passe pas). Elle ancre le cube au sol et l'empêche de « flotter ».
        for (int cy = 0; cy < gh; ++cy)
        {
            for (int cx = 0; cx < gw; ++cx)
            {
                // Pas de bande s'il n'y a pas de cube, ou si un autre cube est juste devant (l'arête n'est pas exposée).
                if (c_[cy * gw + cx] == EMPTY || At(cx, cy + 1) != EMPTY)
                {
                    continue;
                }
                float x0 = ox + cx * cs;
                // y1 : l'arête avant du cube, sur le sol ; d : épaisseur de la bande, 30 % d'une case.
                float y1 = oy + (cy + 1) * cs;
                float d = cs * 0.30f;
                // Un trapèze un peu plus large que le cube (1 puis 2 unités de débord) : le bord est adouci.
                NkVec2f q[4] = {
                    cam.Project(x0 - 1.f, y1, 0),
                    cam.Project(x0 + cs + 1.f, y1, 0),
                    cam.Project(x0 + cs + 2.f, y1 + d, 0),
                    cam.Project(x0 - 2.f, y1 + d, 0)
                };
                // Brun sombre, alpha 50 sur 255.
                dl.AddConvexPolyFilled(q, 4, NkColor(40, 30, 20, 50));
            }
        }
    }

    // Ajoute chaque cube à la file de rendu, avec sa profondeur pour le tri du peintre : le cube de la case i est
    // posé sur la file et DrawPiece(i) sera appelée quand son tour viendra.
    //   q : la file de rendu.   cam : la vue.
    void Emit(RenderQueue& q, const NkView25D& cam) const override
    {
        if (c_.Size() == 0)
        {
            return;
        }
        for (int i = 0; i < gw * gh; ++i)
        {
            if (c_[i] != EMPTY)
            {
                // Profondeur prise au CENTRE de la case, à la hauteur du dessus du cube (ch) : un cube plus bas
                // à l'écran est plus proche et passe devant les cubes plus hauts.
                q.Custom(cam.Depth(0, CellY(i), ch), static_cast<const ICustomDraw*>(this), i);
            }
        }
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
    //   dl : la liste de dessin.   cam : la vue.   i : la case du cube à dessiner.
    void DrawPiece(Dl& dl, const NkView25D& cam, int i) const override
    {
        int cx = i % gw;
        int cy = i / gw;
        NkColor col = palette[c_[i]];
        // (x0, y0) : coin haut gauche de la case ; g : jeu de 0,4 unité de chaque côté, pour qu'on devine la jointure
        // entre deux cubes voisins sur la face avant.
        float x0 = ox + cx * cs;
        float y0 = oy + cy * cs;
        float g = 0.4f;
        // La face avant n'est visible que si aucun cube ne se trouve devant (côté bas de l'écran).
        if (At(cx, cy + 1) == EMPTY)
        {
            // Rectangle de la face avant : du sol (z = 0) jusqu'à la hauteur ch.
            NkVec2f F[4] = {
                cam.Project(x0 + g, y0 + cs, 0),
                cam.Project(x0 + cs - g, y0 + cs, 0),
                cam.Project(x0 + cs - g, y0 + cs, ch),
                cam.Project(x0 + g, y0 + cs, ch)
            };
            // Face avant à 72 % de la teinte du cube : plus sombre que le dessus.
            dl.AddConvexPolyFilled(F, 4, col.Shade(0.72f));
            // La bande basse : le tiers du bas (34 % de la hauteur), encore plus sombre (56 %).
            NkVec2f B[4] = {
                F[0],
                F[1],
                cam.Project(x0 + cs - g, y0 + cs, ch * .34f),
                cam.Project(x0 + g, y0 + cs, ch * .34f)
            };
            // Bande basse : ombre de contact remontée.
            dl.AddConvexPolyFilled(B, 4, col.Shade(0.56f));
        }
        // c : coin coupé de l'octogone, 5 % de la case (coins presque droits) ; b : retrait du biseau intérieur, 15 %
        // de la case (bevel mince) ; cb : coin coupé du biseau, 60 % de c.
        const float c = cs * 0.05f;
        const float b = cs * 0.15f;
        const float cb = c * 0.6f;
        // Le dessus en octogone, point par point, en tournant dans le sens horaire depuis le haut gauche.
        NkVec2f T[8] = {
            cam.Project(x0 + g + c, y0, ch),
            cam.Project(x0 + cs - g - c, y0, ch),
            cam.Project(x0 + cs - g, y0 + c, ch),
            cam.Project(x0 + cs - g, y0 + cs - c, ch),
            cam.Project(x0 + cs - g - c, y0 + cs, ch),
            cam.Project(x0 + g + c, y0 + cs, ch),
            cam.Project(x0 + g, y0 + cs - c, ch),
            cam.Project(x0 + g, y0 + c, ch)
        };
        // Dessus légèrement éclairci : 4 % de blanc mélangé à la couleur.
        dl.AddConvexPolyFilled(T, 8, col.Lerp(NkColor(255, 255, 255, 255), 0.04f));
        // L'octogone intérieur (biseau), même ordre de points.
        NkVec2f I[8] = {
            cam.Project(x0 + b + cb, y0 + b, ch),
            cam.Project(x0 + cs - b - cb, y0 + b, ch),
            cam.Project(x0 + cs - b, y0 + b + cb, ch),
            cam.Project(x0 + cs - b, y0 + cs - b - cb, ch),
            cam.Project(x0 + cs - b - cb, y0 + cs - b, ch),
            cam.Project(x0 + b + cb, y0 + cs - b, ch),
            cam.Project(x0 + b, y0 + cs - b - cb, ch),
            cam.Project(x0 + b, y0 + b + cb, ch)
        };
        // Biseau plus clair (10 % de blanc) : la face paraît bombée.
        dl.AddConvexPolyFilled(I, 8, col.Lerp(NkColor(255, 255, 255, 255), 0.10f));
        // Occlusion : filet sombre sur chaque arête partagée avec un voisin (haut, droite, bas, gauche).
        // Noir à alpha 34 ; l'épaisseur, 0,9 unité, est passée en pixels par l'échelle de la vue.
        const NkColor crease(0, 0, 0, 34);
        const float lw = 0.9f * cam.Scale();
        if (At(cx, cy - 1) != EMPTY)
        {
            dl.AddLine(T[7], T[0], crease, lw);
        }
        if (At(cx + 1, cy) != EMPTY)
        {
            dl.AddLine(T[2], T[3], crease, lw);
        }
        if (At(cx, cy + 1) != EMPTY)
        {
            dl.AddLine(T[4], T[5], crease, lw);
        }
        if (At(cx - 1, cy) != EMPTY)
        {
            dl.AddLine(T[6], T[7], crease, lw);
        }
        // Reflet sur l'arête haute exposée : un liseré clair (blanc, alpha 150) sur l'arête haute de CHAQUE cube,
        // rentré de 10 % de case à chaque bout et placé 0,9 unité sous l'arête, épaisseur 1,1 unité (en pixels via Scale).
        dl.AddLine(
            NkVec2f(T[7].x + cs * .1f * cam.Scale(), T[0].y + 0.9f * cam.Scale()),
            NkVec2f(T[2].x - cs * .1f * cam.Scale(), T[0].y + 0.9f * cam.Scale()),
            NkColor(255, 255, 255, 150),
            1.1f * cam.Scale());
    }

    // Ancien dessin du cube (04/11/2026), gardé et non appelé : « on ne supprime pas ». Raison du retrait : dessus
    // carré sans reflet ni occlusion, le plateau paraissait plat.
    void DrawPieceAncien(Dl& dl, const NkView25D& cam, int i) const
    {
        int cx = i % gw;
        int cy = i / gw;
        NkColor col = palette[c_[i]];
        float x0 = ox + cx * cs;
        float y0 = oy + cy * cs;
        // Jeu en x seulement : pas de trait entre rangées.
        float g = 0.3f;
        // Face avant visible seulement si rien devant.
        if (At(cx, cy + 1) == EMPTY)
        {
            NkVec2f F[4] = {
                cam.Project(x0 + g, y0 + cs, 0),
                cam.Project(x0 + cs - g, y0 + cs, 0),
                cam.Project(x0 + cs - g, y0 + cs, ch),
                cam.Project(x0 + g, y0 + cs, ch)
            };
            dl.AddConvexPolyFilled(F, 4, col.Shade(0.72f));
        }
        NkVec2f T[4] = {
            cam.Project(x0 + g, y0, ch),
            cam.Project(x0 + cs - g, y0, ch),
            cam.Project(x0 + cs - g, y0 + cs, ch),
            cam.Project(x0 + g, y0 + cs, ch)
        };
        dl.AddConvexPolyFilled(T, 4, col.Shade(1.08f));
        // Biseau intérieur plus clair.
        float b = cs * 0.2f;
        NkVec2f I[4] = {
            cam.Project(x0 + b, y0 + b, ch),
            cam.Project(x0 + cs - b, y0 + b, ch),
            cam.Project(x0 + cs - b, y0 + cs - b, ch),
            cam.Project(x0 + b, y0 + cs - b, ch)
        };
        dl.AddConvexPolyFilled(I, 4, col.Shade(1.18f));
    }

private:
    // Chemin : plus court chemin en nombre de cases (parcours en largeur, NkGridPath) puis lissage par ligne de vue.
    // La grille est entourée d'une marge libre de M cases (les fourmis tournent autour du plateau). Les blocs
    // sont les cases bloquées. Le but d'un bloc est l'une de ses 4 cases voisines libres : on essaie les quatre
    // et on garde le plus court.
    //   sx, sy : départ (monde).   target : case du bloc visé (-1 si on vise un point).   tx, ty : le point visé.
    //   out, maxN : tableau de sortie des points de passage et sa taille.   Rend le nombre de points (0 = aucun chemin).
    int Search(float sx, float sy, int target, float tx, float ty, NkVec2f* out, int maxN) const
    {
        // M : marge libre en cases autour du plateau ; PW, PH : taille de la grille de recherche avec ses marges.
        const int M = 8;
        const int PW = gw + 2 * M;
        const int PH = gh + 2 * M;
        nav::NkGridPath grid;
        grid.Init(PW, PH);
        // Chaque cube est une case bloquée (décalée de M à cause de la marge).
        for (int y = 0; y < gh; ++y)
        {
            for (int x = 0; x < gw; ++x)
            {
                if (c_[y * gw + x] != EMPTY)
                {
                    grid.SetBlocked(x + M, y + M, true);
                }
            }
        }
        // Convertit un point du monde en case de la grille de recherche. Le « + 1000 puis - 1000 » donne un arrondi vers
        // le bas même pour les valeurs négatives (un simple (int) arrondirait vers zéro). Résultat borné à la grille.
        auto cellOf = [&](float wx, float wy, int& px, int& py)
        {
            px = NkClamp((int)((wx - ox) / cs + 1000.f) - 1000 + M, 0, PW - 1);
            py = NkClamp((int)((wy - oy) / cs + 1000.f) - 1000 + M, 0, PH - 1);
        };
        int spx;
        int spy;
        cellOf(sx, sy, spx, spy);
        // Le départ est toujours libre : la fourmi peut être contre un cube.
        grid.SetBlocked(spx, spy, false);
        NkVector<nav::NkGridCell> path;
        // (gx, gy) : la case d'arrivée retenue ; -1 tant qu'aucune n'est trouvée.
        int gx = -1;
        int gy = -1;
        if (target >= 0)
        {
            int x = target % gw;
            int y = target / gw;
            // Les 4 voisines du bloc : haut, bas, gauche, droite.
            const int D[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            usize bestLen = 0;
            NkVector<nav::NkGridCell> cur;
            for (int k = 0; k < 4; ++k)
            {
                int ax = x + D[k][0] + M;
                int ay = y + D[k][1] + M;
                // On ignore une voisine bloquée ou sans chemin ; sinon on garde la plus courte.
                if (grid.IsBlocked(ax, ay) || !grid.FindPath(spx, spy, ax, ay, cur, false))
                {
                    continue;
                }
                if (gx < 0 || cur.Size() < bestLen)
                {
                    bestLen = cur.Size();
                    gx = ax;
                    gy = ay;
                }
            }
        }
        else
        {
            // Cible = un point libre : on le convertit en case et on le débloque au cas où.
            cellOf(tx, ty, gx, gy);
            grid.SetBlocked(gx, gy, false);
        }
        // Pas de case d'arrivée ou pas de chemin : on rend 0. Le dernier argument true demande le lissage par ligne de vue.
        if (gx < 0 || !grid.FindPath(spx, spy, gx, gy, path, true))
        {
            return 0;
        }
        int cnt = 0;
        // On convertit chaque case du chemin en point du monde (le centre de la case, sans la marge).
        for (usize i = 0; i < path.Size() && cnt < maxN; ++i)
        {
            out[cnt++] = NkVec2f(ox + ((path[i].x - M) + .5f) * cs, oy + ((path[i].y - M) + .5f) * cs);
        }
        return cnt;
    }

    // Refait le compte des cubes restants, couleur par couleur (après un chargement).
    void Recount()
    {
        for (int i = 0; i < MAX_PAL; ++i)
        {
            left_[i] = 0;
        }
        for (int i = 0; i < gw * gh; ++i)
        {
            if (c_[i] != EMPTY)
            {
                ++left_[c_[i]];
            }
        }
    }

    // c_ : la couleur de chaque case (EMPTY si vide) ; cool_ : le temps de jeu jusqu'auquel la case est en pause ;
    // acc_ : 1 si le cube est accessible (cache de UpdateReach) ; left_ : cubes restants par couleur ;
    // time_ : l'horloge du plateau en secondes ; dirty_ : vrai quand acc_ est périmé.
    NkVector<uint8> c_;
    NkVector<float> cool_;
    NkVector<uint8> acc_;
    int left_[MAX_PAL] = {};
    float time_ = 0;
    bool dirty_ = false;
};

// =============================================================================
//  Hole : trou de nid (décor au sol). Les fourmis y apportent les blocs.
// =============================================================================
class Hole : public Entity {
public:
    // Demi-largeur et demi-hauteur du nid, en unités de plan.
    float rx = 22;
    float ry = 14;

    // Constructeur : marque l'entité comme trou (nid).
    Hole()
    {
        kind = KIND_HOLE;
    }

    // Le nid ne bouge pas : rien à mettre à jour.
    void Update(float) override
    {
    }

    // FOURMILIÈRE (05/11/2026) : un monticule de terre à étages, des grains éparpillés autour et, au centre,
    // l'ouverture sombre où les fourmis disparaissent. Quatre ellipses empilées, de plus en plus petites et claires
    // et décalées vers le haut (la lumière vient du haut) donnent la bosse ; l'ouverture est une ellipse sombre
    // posée en avant de la bosse, avec un cercle encore plus noir pour la profondeur. Les grains sont placés par une
    // suite pseudo-aléatoire de graine fixe (donc identique à chaque trame, rien ne scintille).
    // Dessinée parmi les ombres, donc AVANT les cubes et les fourmis : le nid est un décor posé au sol.
    //   dl : la liste de dessin.   cam : la vue.
    void DrawShadow(Dl& dl, const NkView25D& cam) const override
    {
        NkVec2f c = cam.Project(x, y, 0);
        // Les rayons du nid sont en unités de plan : on les passe en pixels.
        const float rx = this->rx * cam.Scale();
        const float ry = this->ry * cam.Scale();
        // k : l'échelle des grains, proportionnelle à la taille du nid (36 pixels de demi-largeur = grains de taille 1).
        const float k = rx / 36.f;
        // Ombre au sol : une ellipse vert sombre transparente, décalée vers le bas droit.
        dl.AddEllipseFilled(NkVec2f(c.x + rx * .08f, c.y + ry * .22f), rx * 1.42f, ry * 1.38f, NkColor(40, 60, 20, 60));
        // Jupe de terre : la base du monticule, la plus large et la plus sombre.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y + ry * .08f), rx * 1.22f, ry * 1.18f, NkColor(138, 96, 60, 255));
        // Deuxième étage du monticule.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .06f), rx * 1.08f, ry * 1.04f, NkColor(171, 123, 80, 255));
        // Troisième étage.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .22f), rx * .92f, ry * .88f, NkColor(193, 146, 98, 255));
        // Sommet éclairé : le plus clair, décalé vers le haut et la gauche (côté de la lumière).
        dl.AddEllipseFilled(NkVec2f(c.x - rx * .08f, c.y - ry * .38f), rx * .74f, ry * .66f, NkColor(210, 166, 118, 255));
        // Générateur pseudo-aléatoire à graine fixe (77) : les 22 grains tombent toujours aux mêmes endroits.
        NkLcg32 r;
        r.Seed(77u);
        // Grains autour du monticule.
        for (int i = 0; i < 22; ++i)
        {
            // a : angle en radians (0 à 6,27) ; d : distance au centre en rayons (1,12 à 1,51) ;
            // g : rayon du grain, de 1,1 à 2,2 fois k.
            float a = (float)r.NextUInt32(628) * 0.01f;
            float d = 1.12f + (float)r.NextUInt32(40) * 0.01f;
            float g = (1.1f + (float)r.NextUInt32(12) * 0.1f) * k;
            // Trois teintes de terre au hasard : moyenne, claire, foncée.
            int t = (int)r.NextUInt32(3);
            NkColor gc = t == 0 ? NkColor(150, 106, 68, 255) : (t == 1 ? NkColor(189, 143, 96, 255) : NkColor(120, 82, 52, 255));
            dl.AddEllipseFilled(NkVec2f(c.x + NkCos(a) * rx * d, c.y + NkSin(a) * ry * d * 1.02f), g, g * .8f, gc);
        }
        // Lèvre de l'ouverture : l'anneau de terre brune autour du trou.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y + ry * .02f), rx * .58f, ry * .64f, NkColor(94, 62, 40, 255));
        // Ouverture : l'intérieur sombre.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y), rx * .52f, ry * .57f, NkColor(46, 28, 20, 255));
        // Fond noir : l'impression de profondeur.
        dl.AddEllipseFilled(NkVec2f(c.x, c.y - ry * .08f), rx * .36f, ry * .38f, NkColor(20, 11, 8, 255));
    }

    // Le nid n'entre pas dans la file triée : il est entièrement dessiné par DrawShadow, au sol.
    void Emit(RenderQueue&, const NkView25D&) const override
    {
    }
};

// =============================================================================
//  PixelGame : une fourmi ne prend QUE les blocs de SA couleur, les apporte au nid.
// =============================================================================
class PixelGame {
public:
    // Nombre maximal de fourmis gérées en même temps.
    enum { MAX_ANTS = 48 };

    // Prépare l'équipe : mémorise la scène, le plateau et la position du nid, et pose le nid dans la scène.
    //   sc : la scène.   b : le plateau.   nestX, nestY : position du nid (monde).
    void Init(Scene* sc, Board* b, float nestX, float nestY)
    {
        sc_ = sc;
        board_ = b;
        nx_ = nestX;
        ny_ = nestY;
        na_ = 0;
        Hole* h = sc->Add<Hole>();
        h->x = nestX;
        h->y = nestY;
    }

    // Ajoute une fourmi de la couleur de palette `colorIdx`
    //   colorIdx : indice de couleur dans la palette du plateau.   scale : taille relative de la fourmi (1 = normale).
    // Rend la fourmi créée, ou nullptr si la limite est atteinte ou si la couleur n'existe pas.
    Ant* AddAnt(int colorIdx, float scale = 1.0f)
    {
        if (na_ >= MAX_ANTS || colorIdx < 0 || colorIdx >= board_->np)
        {
            return nullptr;
        }
        Ant* a = sc_->Add<Ant>();
        a->colorIdx = colorIdx;
        a->color = board_->palette[colorIdx];
        a->scale = scale;
        // La fourmi disparaît dès qu'elle arrive au nid, dans un rayon de 16 : pas de file d'attente devant le trou.
        a->eatOnArrive = true;
        a->arriveRadius = 16.f;
        // Position de départ : autour du nid, décalée par des formules fixes (aucun hasard) pour que les fourmis ne se
        // superposent pas : x varie de -15 à +15, y de -10 à +10.
        a->x = nx_ + float((na_ * 37) % 31 - 15);
        a->y = ny_ + float((na_ * 53) % 21 - 10);
        // Vitesse de 130 à 169 unités par seconde, variée d'une fourmi à l'autre.
        a->speed = 130.f + float((na_ * 13) % 40);
        ants_[na_++] = a;
        return a;
    }

    // Répartit `total` fourmis proportionnellement au nombre de blocs de chaque couleur (min. 1 par couleur)
    //   total : nombre de fourmis visé (l'arrondi peut le faire varier un peu).   scale : taille relative des fourmis.
    void SpawnByShare(int total, float scale = 1.0f)
    {
        int all = board_->Remaining();
        if (all <= 0)
        {
            return;
        }
        for (int c = 0; c < board_->np; ++c)
        {
            // Part de cette couleur dans le total, arrondie au plus proche, avec au moins une fourmi.
            int n = NkMax(1, (int)((float)total * board_->Remaining(c) / all + 0.5f));
            for (int k = 0; k < n; ++k)
            {
                AddAnt(c, scale);
            }
        }
    }

    // Nombre de fourmis de l'équipe.
    int AntCount() const
    {
        return na_;
    }

    // La fourmi d'indice i (de 0 à AntCount() - 1).
    Ant* AntAt(int i) const
    {
        return ants_[i];
    }

    // Vrai quand le plateau est vide : tous les cubes ont été pris.
    bool Done() const
    {
        return board_->Remaining() == 0;
    }

    // Donne un but à chaque fourmi libre. Appelée à chaque image.
    //   dt : inutilisé (la fonction ne dépend pas du temps ; le paramètre garde la signature des autres Update).
    void Update(float dt)
    {
        for (int i = 0; i < na_; ++i)
        {
            Ant* a = ants_[i];
            // En transport : elle porte un cube, rien à décider.
            if (a->Busy())
            {
                continue;
            }
            if (a->task >= 0)
            {
                // Le cube visé a disparu entre-temps (une autre fourmi l'a pris) : on abandonne ce but.
                if (!board_->Present(a->task, a->colorIdx))
                {
                    a->task = -1;
                }
                else
                {
                    float qx;
                    float qy;
                    board_->ClosestOnCell(a->task, a->x, a->y, qx, qy);
                    // Assez près pour saisir : distance au cube <= rayon de la fourmi + 3 unités de tolérance.
                    if (NkVec2f(qx - a->x, qy - a->y).Len() <= a->Radius() + 3.f)
                    {
                        Grab(a);
                        continue;
                    }
                    if (a->Stuck() >= 3)
                    {
                        // Inaccessible : coincée trois fois, la fourmi change de cible, et ce cube reste 10 s en pause.
                        board_->Cooldown(a->task, 10.f);
                        a->task = -1;
                        a->ResetStuck();
                    }
                    else
                    {
                        a->SetTarget(board_->CellX(a->task), board_->CellY(a->task));
                    }
                    if (a->task >= 0)
                    {
                        continue;
                    }
                }
            }
            // Pas de but : on cherche le cube de SA couleur le plus proche.
            int c = board_->FindNearest(a->colorIdx, a->x, a->y);
            if (c >= 0)
            {
                a->task = c;
                a->ResetStuck();
                a->SetTarget(board_->CellX(c), board_->CellY(c));
                // Cooldown de 0 : on rafraîchit la case sans la bloquer.
                board_->Cooldown(c, 0.f);
            }
            else if (a->Arrived())
            {
                // Rien à prendre : elle attend vers le nid, à un point fixe propre à chaque fourmi.
                a->SetTarget(nx_ + float((i * 71) % 120 - 60), ny_ + float((i * 29) % 60 - 20));
            }
        }
        (void)dt;
    }

private:
    // La fourmi a atteint son cube : on le retire du plateau, on crée le bloc porté, et la fourmi part au nid.
    void Grab(Ant* a)
    {
        int cell = a->task;
        float cx = board_->CellX(cell);
        float cy = board_->CellY(cell);
        int col = board_->Take(cell);
        // Le bloc porté a la taille d'une case et la couleur du cube pris, posé à la place de la case.
        Box* b = sc_->Add<Box>();
        b->d = board_->cs;
        b->w = b->d;
        b->h = board_->ch;
        b->color = board_->palette[col];
        // Pas de chiffre sur le bloc ; il est dessiné à 80 % de sa taille quand la fourmi le porte.
        b->showText = false;
        b->carryScale = 0.8f;
        b->x = cx;
        b->y = cy;
        a->task = -1;
        a->PickUp(b, nx_, ny_);
    }

    // Scène, plateau, les fourmis et leur nombre, et la position du nid.
    Scene* sc_ = nullptr;
    Board* board_ = nullptr;
    Ant* ants_[MAX_ANTS];
    int na_ = 0;
    float nx_ = 0;
    float ny_ = 0;
};

} // namespace nk25d
