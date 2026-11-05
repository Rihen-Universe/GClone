// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25D.h — Fourmis + boîtes 2.5D procédurales, collisions, portage de blocs.
// Migré le 04/11/2026 sur Nkentseu : plus aucune bibliothèque standard, plus de couche maison pour ce que le
// moteur fournit déjà (NkVec2f, NkColor, NkSin..., NkVector, NkUniquePtr, NkStableSort, NkConvexHull2D, NkLcg32,
// NkGuiDrawList). Reste ici ce qui est propre au jeu : la caméra 2.5D, les entités, la file de rendu.
#pragma once
#include "NKMath/NKMath.h"
#include "NKMath/NkConvexHull2D.h"
#include "NKMath/NkRandom.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Utilities/NkSort.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NkView25D.h"

namespace nk25d {
using namespace nkentseu;
using namespace nkentseu::math;
using memory::NkUniquePtr;
using memory::NkMakeUnique;

using Dl = nkgui::NkGuiDrawList;

// Police du jeu : posée une fois par l'application (les polices embarquées de NKFont, chargées par NkCanvasGuiApp).
// Le texte est un détail d'interface : le passer en paramètre de chaque primitive alourdirait toutes les signatures.
inline nkgui::NkGuiFont*& GameFont() { static nkgui::NkGuiFont* f = nullptr; return f; }

// =============================================================================
//  VUE 2.5D : NkView25D (NkView25D.h), qui hérite de renderer::NkView2D
// =============================================================================
// La caméra 2.5D (projection oblique, hauteur z, profondeur de tri, ombre portée) et son modèle mathématique complet
// vivent dans NkView25D.h, écrit SANS dépendance au jeu pour monter tel quel dans NKCanvas. Elle remplace la structure
// Camera du 04/11/2026 (Rodolf, 05/11/2026) : `center` et `size`, hérités de NkView2D, remplacent `origin` et portent
// l'adaptation à l'écran. Ci-dessous, l'ANCIENNE formule, gardée (« on ne supprime pas ») pour le banc : avec une vue
// neutre, NkView25D::Project doit rendre exactement les mêmes points qu'elle (voir VerifierVue dans main.cpp).
struct CameraAncienne {
    NkVec2f origin{0.f, 0.f};
    float   tilt = 0.65f;                 // tan(phi) : 0.65 correspond à phi = 33 degrés
    NkVec2f lightDir{0.55f, 0.30f};
    NkVec2f Project(float x, float y, float z) const { return NkVec2f(origin.x + x, origin.y + y * tilt - z); }
    NkVec2f ShadowPos(float x, float y, float z) const { return Project(x + lightDir.x * z, y + lightDir.y * z, 0.f); }
    float   Depth(float, float y, float z) const { return y + z * tilt; }
};

// Texte centré en (cx, cy), de hauteur `size` (pixels), avec une ombre portée optionnelle.
inline void GameText(Dl& dl, float cx, float cy, float size, const char* t, const NkColor& c, bool shadow = true) {
    if (!GameFont()) return;
    if (shadow) renderer::NkTexteCentreTaille(dl, GameFont(), cx + size * .05f, cy - size * .5f + size * .08f, t, NkColor(0, 0, 0, 90), size);
    renderer::NkTexteCentreTaille(dl, GameFont(), cx, cy - size * .5f, t, c, size);
}

// Texte avec CONTOUR (les nombres de l'interface) : le même texte, dans la couleur du contour, est posé huit fois
// autour de la position (aux huit directions, à `epaisseur` pixels), puis le texte de la couleur voulue par-dessus.
// C'est le « dilatation morphologique » le plus simple : le contour est l'union des huit copies décalées. Huit
// décalages suffisent tant que l'épaisseur reste petite devant la taille du texte (ici 7 % de la hauteur).
inline void GameTextOutline(Dl& dl, float cx, float cy, float size, const char* t, const NkColor& c, const NkColor& outline) {
    if (!GameFont()) return;
    const float o = NkMax(0.8f, size * 0.07f);
    static const float dx[8] = {-1, 1, 0, 0, -.7f, .7f, -.7f, .7f}, dy[8] = {0, 0, -1, 1, -.7f, -.7f, .7f, .7f};
    for (int i = 0; i < 8; ++i) renderer::NkTexteCentreTaille(dl, GameFont(), cx + dx[i] * o, cy - size * .5f + dy[i] * o, t, outline, size);
    renderer::NkTexteCentreTaille(dl, GameFont(), cx, cy - size * .5f, t, c, size);
}

// =============================================================================
//  SPHÈRE : trois disques qui suggèrent le volume
// =============================================================================
// En projection orthographique, une sphère de rayon r se projette en un DISQUE de rayon r centré sur la
// projection de son centre, quelle que soit l'inclinaison : c'est exact, pas une approximation. Reste à lui
// donner du volume par l'ombrage. Au lieu de calculer un éclairage par pixel (loi de Lambert, intensité
// proportionnelle à n . l), on superpose trois formes, du plus sombre au plus clair, décalées vers la lumière
// (en haut à gauche) :
//   1. un disque entier sombre (teinte x 0.60) : la zone d'ombre propre, tout le contour ;
//   2. un disque un peu plus petit (0.88 r) décalé vers la lumière, de la teinte de base : la partie éclairée ;
//   3. une petite ellipse très claire (teinte x 1.35) : le reflet brillant (terme spéculaire).
// Le décalage des centres crée un croissant sombre côté opposé à la lumière, ce qui se lit comme un volume.
// Contour fin (ajouté le 05/11/2026) : un disque un peu plus grand (r + e) de la teinte x 0.38, dessiné DESSOUS.
// Il sépare nettement deux fourmis qui se serrent (sans lui, deux corps de la même couleur fusionnent en une
// tache) et garde lisible une fourmi claire sur l'assiette claire. e vaut au moins 0.7 px : sous cette valeur
// le liseré disparaîtrait à petite taille.
inline void DrawSphere(Dl& dl, const NkView25D& cam, float x, float y, float z, float r, const NkColor& base) {
    NkVec2f c = cam.Project(x, y, z);
    dl.AddEllipseFilled(c, r + NkMax(0.7f, r * .11f), r + NkMax(0.7f, r * .11f), base.Shade(0.38f));
    dl.AddEllipseFilled(c, r, r, base.Shade(0.60f));
    dl.AddEllipseFilled(NkVec2f(c.x - r * .10f, c.y - r * .12f), r * .88f, r * .88f, base);
    dl.AddEllipseFilled(NkVec2f(c.x - r * .30f, c.y - r * .35f), r * .40f, r * .32f, base.Shade(1.35f));
}

// =============================================================================
//  FILE DE RENDU : des commandes simples, triées par profondeur
// =============================================================================
// Chaque entité « émet » ses morceaux dessinables (sphères, pattes, yeux...) avec leur profondeur ; la scène
// trie ensuite TOUTES les commandes ensemble et les dessine dans l'ordre. C'est l'algorithme du peintre à
// l'échelle des morceaux et non des objets : la tête d'une fourmi peut passer devant le corps d'une autre tout
// en restant derrière sa propre antenne. Le tri est STABLE (NkStableSort, tri fusion) : deux commandes de même
// profondeur gardent leur ordre d'émission, donc l'image ne scintille pas d'une trame à l'autre.
struct ICustomDraw { virtual void DrawPiece(Dl& dl, const NkView25D& cam, int idx) const = 0; virtual ~ICustomDraw() {} };

enum CmdKind { CMD_LINE, CMD_ELLIPSE, CMD_SPHERE, CMD_BOX, CMD_TAPER, CMD_CUSTOM };
struct DrawCmd {
    float depth; int kind;
    NkVec2f p0, p1; float a, b; NkColor col; const void* ptr;
};
struct RenderQueue {
    NkVector<DrawCmd>& cmds;
    void Push(const DrawCmd& c) { cmds.PushBack(c); }
    void Line(float dp, NkVec2f a, NkVec2f b, float w, NkColor c)    { Push({dp, CMD_LINE, a, b, w, 0, c, nullptr}); }
    void Taper(float dp, NkVec2f a, NkVec2f b, float w0, float w1, NkColor c) { Push({dp, CMD_TAPER, a, b, w0, w1, c, nullptr}); }
    void Ellipse(float dp, NkVec2f c0, float rx, float ry, NkColor c) { Push({dp, CMD_ELLIPSE, c0, NkVec2f(), rx, ry, c, nullptr}); }
    void Sphere(float dp, float x, float y, float z, float r, NkColor c) { Push({dp, CMD_SPHERE, NkVec2f(x, y), NkVec2f(), z, r, c, nullptr}); }
    void Custom(float dp, const void* iface, int idx) { Push({dp, CMD_CUSTOM, NkVec2f(), NkVec2f(), (float)idx, 0, NkColor(0, 0, 0, 0), iface}); }
    void BoxCmd(float dp, const void* b) { Push({dp, CMD_BOX, NkVec2f(), NkVec2f(), 0, 0, NkColor(0, 0, 0, 0), b}); }
};

enum EntityKind { KIND_ANT, KIND_BOX, KIND_BOARD, KIND_HOLE };
struct Entity {
    float x = 0, y = 0; int kind = 0; bool dead = false;
    virtual ~Entity() {}
    // Obstacle : repousse un cercle hors de l'entité ; renvoie true + normale si contact
    virtual bool ResolveCircle(float&, float&, float, float&, float&) { return false; }
    virtual void Update(float dt) = 0;
    virtual void LateUpdate() {}
    virtual void DrawShadow(Dl&, const NkView25D&) const = 0;
    virtual void Emit(RenderQueue&, const NkView25D&) const = 0;
};

class Ant;

// =============================================================================
//  BOX : bloc 3D orientable, déplaçable, portable (se réduit quand porté)
// =============================================================================
// Un pavé de largeur w, profondeur d, hauteur h, tourné de `yaw` autour de l'axe vertical. Ses quatre coins au sol
// se calculent par la rotation 2D (x', y') = (lx cos yaw - ly sin yaw, lx sin yaw + ly cos yaw). Seules les faces
// tournées vers la caméra sont dessinées (élimination des faces cachées, « back-face culling ») : la normale
// sortante d'un côté (a -> b) est (b.y - a.y, -(b.x - a.x)) ; si sa composante y est négative, la face regarde
// vers le fond de l'écran et on l'ignore. L'éclairage de chaque face est ensuite lambertien simplifié :
// 0.5 + 0.35 * (n . l), borné, où l est la direction vers la lumière.
class Box : public Entity {
public:
    float w = 56, d = 44, h = 22, yaw = 0;
    int   value = 0;
    NkColor color{80, 200, 240, 255};
    bool  mystery = false, faceMotion = false, showText = true;
    float carryScale = 0.38f;
    float lift = 0;                      // hauteur ajoutée (portage)
    bool  carried = false;
    Ant*  carrier = nullptr;

    Box() { kind = KIND_BOX; }
    void Press(float s = 8)   { vel_ -= s * 20; }
    void MoveTo(float tx, float ty) { tx_ = tx; ty_ = ty; moving_ = true; }   // x,y = centre
    void RotateTo(float a)    { yawT_ = a; }
    void SetYaw(float a)      { yaw = yawT_ = a; }
    void SetCarried(bool c)   { carried = c; scT_ = c ? carryScale : 1.f; if (c) { moving_ = false; hop_ = 0; } else { carrier = nullptr; lift = 0; } }
    float Scale() const { return sc_; }

    // Point de l'empreinte le plus proche de (px,py) (pour collisions / ramassage) : on exprime le point dans le
    // repère local du pavé (rotation inverse), on le borne au rectangle, puis on revient au repère monde.
    void Closest(float px, float py, float& cx, float& cy) const {
        float cs = NkCos(yaw), sn = NkSin(yaw), dx = px - x, dy = py - y;
        float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;
        float hx = w * sc_ * .5f, hy = d * sc_ * .5f;
        float qx = NkClamp(lx, -hx, hx), qy = NkClamp(ly, -hy, hy);
        cx = x + qx * cs - qy * sn; cy = y + qx * sn + qy * cs;
    }

    void Update(float dt) override {
        // Lissage exponentiel : v += (cible - v) * (1 - e^(-k dt)). Indépendant du pas de temps (contrairement à
        // v += (cible - v) * k * dt qui diverge si k dt > 1), et sans dépassement.
        sc_ += (scT_ - sc_) * (1 - NkExp(-10 * dt));
        if (!carried) {
            if (moving_) {
                float dx = tx_ - x, dy = ty_ - y, dist = NkVec2f(dx, dy).Len(), k = 1 - NkExp(-12 * dt);
                x += dx * k; y += dy * k;
                hop_ = NkMin(dist * 0.25f, 14.f);
                if (faceMotion && dist > 6) yawT_ = NkAtan2(dy, dx);
                if (dist < .3f) { x = tx_; y = ty_; moving_ = false; hop_ = 0; }
            }
            yaw += NkAngle::FromRad(yawT_ - yaw).Rad() * (1 - NkExp(-14 * dt));
        }
        // Ressort amorti (écrasement à l'appui) : accélération = -k x - c v, intégrée à pas explicite.
        vel_ += (-180 * squash_ - 14 * vel_) * dt;
        squash_ += vel_ * dt;
    }

    // Ombre : enveloppe convexe de la base et du dessus décalé par la lumière (NkConvexHull2D).
    void DrawShadow(Dl& dl, const NkView25D& cam) const override {
        float top = h * sc_ + hop_ + lift;
        NkVector<NkVec2f> pts, hull;
        for (int i = 0; i < 4; ++i) {
            NkVec2f c = Corner(i);
            pts.PushBack(cam.Project(c.x, c.y, 0)); pts.PushBack(cam.ShadowPos(c.x, c.y, top));
        }
        NkConvexHull2D(pts, hull);
        if (hull.Size() >= 3) dl.AddConvexPolyFilled(hull.Data(), (int32)hull.Size(), NkColor(0, 0, 0, carried ? 35 : 55));
    }

    void Emit(RenderQueue& q, const NkView25D& cam) const override {
        float hh = NkMax(4.f, h * sc_);
        q.BoxCmd(cam.Depth(x, y, hop_ + lift + hh), this);
    }

    void DrawBody(Dl& dl, const NkView25D& cam) const {
        float z0 = hop_ + lift, hh = NkMax(4.f * sc_, h * sc_ * (1 - squash_ * 0.012f)), z1 = z0 + hh;
        NkVec2f c[4]; for (int i = 0; i < 4; ++i) c[i] = Corner(i);
        NkVec2f lv(-cam.lightDir.x, -cam.lightDir.y); lv = lv / lv.Len();
        for (int i = 0; i < 4; ++i) {
            NkVec2f a = c[i], b = c[(i + 1) & 3];
            NkVec2f nv(b.y - a.y, -(b.x - a.x)); nv = nv / nv.Len();
            if (nv.y <= 0.001f) continue;                                    // face tournée vers le fond : cachée
            float lit = NkClamp(0.5f + 0.35f * (nv.x * lv.x + nv.y * lv.y), 0.4f, 0.95f);
            NkVec2f q[4] = { cam.Project(a.x, a.y, z0), cam.Project(b.x, b.y, z0), cam.Project(b.x, b.y, z1), cam.Project(a.x, a.y, z1) };
            dl.AddConvexPolyFilled(q, 4, color.Shade(lit));
            dl.AddLine(q[3], q[2], color.Shade(1.35f), 1.5f * sc_ * cam.Scale());
        }
        NkVec2f T[4], I[4]; float k = 1 - 8.f / NkMin(w, d);
        for (int i = 0; i < 4; ++i) {
            T[i] = cam.Project(c[i].x, c[i].y, z1);
            I[i] = cam.Project(x + (c[i].x - x) * k, y + (c[i].y - y) * k, z1);
        }
        dl.AddConvexPolyFilled(T, 4, color.Shade(1.30f));
        dl.AddConvexPolyFilled(I, 4, color.Shade(1.12f));
        if (showText && sc_ > 0.6f) {
            NkString s = mystery ? NkString("?") : NkFormat("{0}", value);
            NkVec2f t = cam.Project(x, y, z1);
            GameText(dl, t.x + 1, t.y + 2, 22 * sc_ * cam.Scale(), s.CStr(), NkColor(0, 0, 0, 90), false);
            GameText(dl, t.x, t.y, 22 * sc_ * cam.Scale(), s.CStr(), NkColor(255, 255, 255, 255), false);
        }
    }

private:
    NkVec2f Corner(int i) const {
        static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
        float lx = sx[i] * w * sc_ * .5f, ly = sy[i] * d * sc_ * .5f, cs = NkCos(yaw), sn = NkSin(yaw);
        return NkVec2f(x + lx * cs - ly * sn, y + lx * sn + ly * cs);
    }
    float tx_ = 0, ty_ = 0, hop_ = 0, squash_ = 0, vel_ = 0, yawT_ = 0, sc_ = 1, scT_ = 1;
    bool  moving_ = false;
};

// =============================================================================
//  ANT : fourmi procédurale, steering, évitement, ramasse/porte/dépose un Box
// =============================================================================
// Déplacement par « steering » (Reynolds, 1999) : à chaque pas, la fourmi calcule la direction voulue (vers le
// prochain point de passage), tourne vers elle à vitesse angulaire bornée (8 rad/s) et avance à une vitesse
// réduite quand elle est mal alignée : v = vitesse * max(0.2, cos(écart d'angle)). Évitement : lors d'une
// collision la scène appelle Nudge, qui ajoute un vecteur tangent à l'obstacle (côté de la cible). Impasse :
// si la fourmi progresse de moins de 8 % de sa vitesse sur 0.5 s, elle exécute une manoeuvre de dégagement.
class Ant : public Entity {
public:
    NkColor color{225, 40, 60, 255};
    float speed = 70, heading = 0, scale = 1.0f;
    int   colorIdx = -1, task = -1, owner = -1;      // jeu : couleur de la fourmi / cellule visée
    float arriveRadius = 3;              // distance à laquelle la cible est considérée atteinte
    bool  eatOnArrive = false;           // détruit le bloc porté à l'arrivée (jeu)
    int   legPairs = 3;                  // 3 = fourmi (6 pattes) ; 4 = 8 pattes

    // Graine : l'adresse de l'objet (chaque fourmi a sa suite, rejouable avec Seed)
    Ant() { kind = KIND_ANT; rng_.Seed((uint32)((usize)this >> 4) * 2654435761u | 1u); }
    float Radius() const { return 8.f * scale; }
    void SetTarget(float tx, float ty) { tx_ = tx; ty_ = ty; hasTarget_ = true; nwp_ = iwp_ = 0; }
    // Chemin : points de passage puis cible finale (calculé par Board::FindPath*)
    void SetPath(const NkVec2f* p, int n, float fx, float fy) {
        nwp_ = n > 160 ? 160 : n; for (int i = 0; i < nwp_; ++i) wp_[i] = p[i];
        iwp_ = 0; tx_ = fx; ty_ = fy; hasTarget_ = true;
    }
    bool Arrived() const { return !hasTarget_; }
    int  WpCount() const { return nwp_; }
    int  WpIndex() const { return iwp_; }
    NkVec2f Wp(int i) const { return wp_[i]; }
    NkVec2f Target() const { return NkVec2f(tx_, ty_); }
    bool Busy() const { return state_ != 0; }
    int  Stuck() const { return stuck_; }
    void ResetStuck() { stuck_ = 0; esc_ = 0; }
    // Ramassage direct d'un bloc (jeu) puis transport vers (dx,dy)
    void PickUp(Box* b, float dx, float dy) { box_ = b; b->SetCarried(true); b->carrier = this; state_ = 2; SetTarget(dx, dy); ResetStuck(); }
    Box* Carried() const { return state_ == 2 ? box_ : nullptr; }

    // Ordre : aller chercher b, puis le déposer vers (dx,dy)
    void Fetch(Box* b, float dx, float dy) { ResetStuck(); box_ = b; dropX_ = dx; dropY_ = dy; state_ = 1; SetTarget(b->x, b->y); }

    // Appelé par Scene lors d'une collision : dévie le long de l'obstacle
    void Nudge(float nx, float ny) {
        float tx = -ny, ty = nx, wx = tx_ - x, wy = ty_ - y;
        if (tx * wx + ty * wy < 0) { tx = -tx; ty = -ty; }
        avX_ = tx; avY_ = ty; avT_ = 0.4f;
    }

    // Événement « bloc mangé » : levé à l'arrivée au nid, lu puis baissé par le jeu (miettes, rebond du panier).
    bool    justAte = false;
    // « Fantôme » (secondes restantes) : une fourmi coincée traverse les autres fourmis (jamais les cubes). Sans cela deux
    // fourmis qui se croisent en sens inverse dans une rainure d'une case se bloquent à jamais (blocage mutuel).
    float   ghost = 0;
    int     jams = 0;                    // nombre de blocages constatés (le jeu recale la fourmi au centre d'une case libre au troisième)
    int     trips = 0;                   // nombre de blocs mangés : une fourmi correcte n'en mange qu'un (règle du 05/11)
    NkColor ateColor{255, 255, 255, 255};

    void Update(float dt) override {
        t_ += dt; born_ += dt; if (ghost > 0) ghost -= dt;
        if (state_ == 1) {
            if (!box_ || box_->carried) { state_ = 0; box_ = nullptr; hasTarget_ = false; }
            else {
                float cx, cy; box_->Closest(x, y, cx, cy);
                if (NkVec2f(cx - x, cy - y).Len() <= Radius() + 3.f) {
                    box_->SetCarried(true); box_->carrier = this; state_ = 2; SetTarget(dropX_, dropY_); ResetStuck();
                } else SetTarget(box_->x, box_->y);
            }
        } else if (state_ == 2 && !hasTarget_) {
            if (eatOnArrive) { justAte = true; ++trips; ateColor = box_->color; box_->SetCarried(false); box_->dead = true; box_ = nullptr; state_ = 0; ResetStuck(); }
            else {
                float fx = NkCos(heading), fy = NkSin(heading);
                box_->SetCarried(false); box_->x = x + fx * 30 * scale; box_->y = y + fy * 30 * scale; box_->SetYaw(heading);
                box_ = nullptr; state_ = 0;
            }
        }
        if (hasTarget_) {
            const float wpTol = 7.f;                           // tolérance d'un point de passage (px)
            // Point de passage atteint à wpTol ; le DERNIER est atteint à `arriveRadius` (16 px au nid) : sans cela deux fourmis qui visent le
            // même point final se repoussent (collision) et aucune ne s'approche à 7 px : blocage mutuel à l'entrée du nid (mesuré par le banc).
            while (iwp_ < nwp_ && NkVec2f(wp_[iwp_].x - x, wp_[iwp_].y - y).Len() < (iwp_ == nwp_ - 1 ? NkMax(wpTol, arriveRadius) : wpTol)) ++iwp_;
            float fd = NkVec2f(tx_ - x, ty_ - y).Len();
            if (iwp_ >= nwp_ && fd < arriveRadius) { hasTarget_ = false; stuckAcc_ = 0; }
            else {
                float gx = iwp_ < nwp_ ? wp_[iwp_].x : tx_, gy = iwp_ < nwp_ ? wp_[iwp_].y : ty_;
                float dx = gx - x, dy = gy - y, dist = NkMax(1e-3f, NkVec2f(dx, dy).Len());
                // Détection d'impasse : peu de progrès sur 0.5 s -> manoeuvre de dégagement (latérale + recul)
                stuckAcc_ += dt;
                if (stuckAcc_ >= 0.5f) {
                    if (esc_ <= 0 && NkVec2f(x - sx_, y - sy_).Len() < speed * 0.08f) { esc_ = 0.9f; escSign_ = rng_.NextUInt32(2) ? 1.f : -1.f; ++stuck_; }
                    sx_ = x; sy_ = y; stuckAcc_ = 0;
                }
                float vx = dx / dist, vy = dy / dist;
                if (esc_ > 0) { esc_ -= dt; float ex = -vy * escSign_ - vx * 0.35f, ey = vx * escSign_ - vy * 0.35f; vx = ex; vy = ey; }
                else if (avT_ > 0) { float k = 1.6f * (avT_ / 0.4f); vx += avX_ * k; vy += avY_ * k; avT_ -= dt; }
                float diff = NkAngle::FromRad(NkAtan2(vy, vx) - heading).Rad();
                heading = NkAngle::FromRad(heading + NkClamp(diff, -8 * dt, 8 * dt)).Rad();
                float v = speed * NkMax(0.2f, NkCos(diff));
                x += NkCos(heading) * v * dt; y += NkSin(heading) * v * dt;
                phase_ += v * dt * 0.28f;
            }
        } else stuckAcc_ = 0;
        LateUpdate();
    }
    void LateUpdate() override {                                              // le bloc suit APRÈS les collisions
        if (state_ == 2 && box_) {
            box_->x = x + NkCos(heading) * scale; box_->y = y + NkSin(heading) * scale;
            box_->lift = 14.f * scale; box_->SetYaw(heading);
        }
    }

    void DrawShadow(Dl& dl, const NkView25D& cam) const override {
        float s = scale * (0.25f + 0.75f * NkMin(1.f, born_ / 0.30f)), fx = NkCos(heading), fy = NkSin(heading);
        const float off[2] = {-6.5f, 5.5f}, rad[2] = {7.5f, 8.5f};
        for (int i = 0; i < 2; ++i) {
            NkVec2f c = cam.ShadowPos(x + fx * off[i] * s, y + fy * off[i] * s, 6 * s);
            dl.AddEllipseFilled(c, rad[i] * s * cam.Scale(), rad[i] * s * cam.tilt * 1.1f * cam.Scale(), NkColor(0, 0, 0, 55));
        }
    }

    // Fourmi du 05/11/2026 : trois segments nets (abdomen, thorax, tête) cerclés d'un contour fin, six pattes FINES et
    // sombres qui marchent en TRIPODE, deux antennes brisées, deux yeux. L'objectif est la lisibilité À PETITE TAILLE
    // (une fourmi mesure ~20 px) et quand elles se serrent : le contour de chaque disque et le contraste des
    // pattes sombres sur le corps coloré évitent la « tache » de l'ancien rendu (voir EmitAncien, plus bas).
    //
    // MARCHE EN TRIPODE : les insectes ne lèvent jamais deux pattes voisines ensemble. À chaque instant, trois
    // pattes sont en appui (le triangle de sustentation, stable) pendant que les trois autres avancent : avant gauche,
    // milieu droite et arrière gauche forment le tripode A ; avant droite, milieu gauche et arrière droite, le B.
    // La phase de la patte i du côté `side` vaut donc  phase + PI * ((i + [side > 0]) mod 2) :
    // deux pattes voisines d'un même côté sont en opposition, et deux pattes d'un même rang aussi.
    void Emit(RenderQueue& q, const NkView25D& cam) const override {
        const float born = NkMin(1.f, born_ / 0.30f);                       // apparition en douceur : 0 -> 1 en 0.3 s
        const float s = scale * (0.25f + 0.75f * (1.f - (1.f - born) * (1.f - born) * (1.f - born)));   // sortie douce (cubique)
        const float fx = NkCos(heading), fy = NkSin(heading), px = -fy, py = fx;
        const float zb = 6.f * s + NkSin(phase_ * 2) * 0.5f * s;
        const NkColor leg = color.Shade(0.42f);

        const float hOff = 6.4f * s, hR = 7.8f * s, hZ = zb + s;
        const float off[3] = {-7.0f, -0.2f, 6.4f}, rad[3] = {7.4f, 4.3f, 7.8f}, zo[3] = {0.f, 0.5f, 1.f};
        for (int i = 0; i < 3; ++i) {
            float wx = x + fx * off[i] * s, wy = y + fy * off[i] * s, wz = zb + zo[i] * s;
            q.Sphere(cam.Depth(wx, wy, wz), wx, wy, wz, rad[i] * s, color);
        }
        const float hx = x + fx * hOff, hy = y + fy * hOff;
        const float headDepth = cam.Depth(0, hy, hZ);

        for (int side = -1; side <= 1; side += 2)
            for (int i = 0; i < 3; ++i) {
                float u = i - 1.f;                                              // -1 arrière, 0 milieu, +1 avant
                float ph = phase_ + ((i + (side > 0 ? 1 : 0)) & 1) * NK_PI_F;   // tripode (voir l'en-tête)
                float stride = NkSin(ph) * 2.4f * s, lf = NkMax(0.f, NkCos(ph)) * 2.6f * s;   // balancement et levée du pied
                float x0 = x + fx * (u * 3.2f - 0.6f) * s, y0 = y + fy * (u * 3.2f - 0.6f) * s;
                float reach = (11.0f - 1.6f * (u < 0 ? -u : u)) * s;
                float ex = x0 + px * side * reach + fx * (u * 4.4f * s + stride);
                float ey = y0 + py * side * reach + fy * (u * 4.4f * s + stride);
                float kx = x0 + px * side * reach * .5f + fx * (u * 1.6f * s + stride * .5f);
                float ky = y0 + py * side * reach * .5f + fy * (u * 1.6f * s + stride * .5f);
                NkVec2f A = cam.Project(x0, y0, zb - 1.0f * s), K = cam.Project(kx, ky, zb + 2.2f * s + lf), F = cam.Project(ex, ey, lf);
                float dp = cam.Depth(0, (y0 + ky + ey) / 3.f, zb - 2.f * s);
                q.Taper(dp, A, K, 2.1f * s, 1.7f * s, leg);
                q.Taper(dp, K, F, 1.7f * s, 0.9f * s, leg);
                q.Ellipse(dp, F, 1.0f * s, 1.0f * s * cam.tilt + .2f * s, leg);
            }

        // Antennes : deux segments (tige puis coude), fines et sombres, et un petit bouton au bout.
        float sway = NkSin(t_ * 5) * 0.8f * s;
        for (int side = -1; side <= 1; side += 2) {
            float bx = hx + (fx * 3.0f + px * side * 2.6f) * s, by = hy + (fy * 3.0f + py * side * 2.6f) * s, bz = hZ + 5.4f * s;
            float mx = bx + (fx * 4.0f + px * side * 2.0f) * s, my = by + (fy * 4.0f + py * side * 2.0f) * s;
            float tx = mx + (fx * 2.0f + px * side * 4.0f) * s + px * side * sway, ty = my + (fy * 2.0f + py * side * 4.0f) * s + py * side * sway;
            NkVec2f B = cam.Project(bx, by, bz), M = cam.Project(mx, my, bz + 3.0f * s), T = cam.Project(tx, ty, bz + 2.6f * s);
            float dp = headDepth + hR * 1.1f;
            q.Taper(dp, B, M, 1.5f * s, 1.2f * s, leg);
            q.Taper(dp, M, T, 1.2f * s, 0.9f * s, leg);
            q.Ellipse(dp, T, 1.1f * s, 1.1f * s, leg);
        }

        // Yeux : deux billes blanches cerclées de sombre, pupille vers l'avant.
        for (int side = -1; side <= 1; side += 2) {
            NkVec2f e = cam.Project(hx + (fx * 3.6f + px * side * 3.9f) * s, hy + (fy * 3.6f + py * side * 3.9f) * s, hZ + 4.4f * s);
            NkVec2f pu = cam.Project(hx + (fx * 5.0f + px * side * 3.9f) * s, hy + (fy * 5.0f + py * side * 3.9f) * s, hZ + 4.4f * s);
            float dp = headDepth + hR * 0.95f;
            q.Ellipse(dp, e, 2.5f * s, 2.7f * s, NkColor(30, 20, 25, 255));
            q.Ellipse(dp + 0.01f, e, 1.9f * s, 2.1f * s, NkColor(255, 255, 255, 255));
            q.Ellipse(dp + 0.02f, NkVec2f(e.x + (pu.x - e.x) * .6f, e.y + (pu.y - e.y) * .6f + .2f * s), 1.0f * s, 1.2f * s, NkColor(25, 15, 20, 255));
        }
    }

    // Emit ANCIEN (04/11/2026), gardé et non appelé : « on ne supprime pas ». Raison du retrait : pattes et antennes
    // épaisses, de la couleur du corps et sans contour, qui fusionnaient en tache quand les fourmis se serrent.
    // Style "chibi" : grosse tête + abdomen, oreilles-antennes épaisses, pattes courtes, gros yeux.
    void EmitAncien(RenderQueue& q, const NkView25D& cam) const {
        const float s = scale, fx = NkCos(heading), fy = NkSin(heading), px = -fy, py = fx;
        const float zb = 6.f * s + NkSin(phase_ * 2) * 0.5f * s;
        const NkColor leg = color.Shade(0.82f), hi = color.Shade(1.35f);

        // Corps : abdomen, petite taille (cache la jonction), grosse tête — profondeur = centre de la sphère
        const float hOff = 5.5f * s, hR = 8.5f * s, hZ = zb + s;
        const float off[3] = {-6.5f, -0.5f, 5.5f}, rad[3] = {7.5f, 4.2f, 8.5f}, zo[3] = {0.f, 0.5f, 1.f};
        for (int i = 0; i < 3; ++i) {
            float wx = x + fx * off[i] * s, wy = y + fy * off[i] * s, wz = zb + zo[i] * s;
            q.Sphere(cam.Depth(wx, wy, wz), wx, wy, wz, rad[i] * s, color);
        }
        const float hx = x + fx * hOff, hy = y + fy * hOff;
        const float headDepth = cam.Depth(0, hy, hZ);

        // Pattes courtes et épaisses, même couleur que le corps, attachées à la taille (3 paires).
        // Marche en tripode : les pattes d'un côté sont en opposition de phase avec celles de l'autre (+ PI).
        const int n = legPairs < 3 ? 3 : (legPairs > 4 ? 4 : legPairs);
        for (int side = -1; side <= 1; side += 2)
            for (int i = 0; i < n; ++i) {
                float u = i - (n - 1) * .5f, un = u / ((n - 1) * .5f);          // -1 arrière .. +1 avant
                float along = (u * 3.4f - 1.f) * s;
                float ph = phase_ + i * 2.09f + (side > 0 ? NK_PI_F : 0);
                float stride = NkSin(ph) * 2.2f * s, lf = NkMax(0.f, NkCos(ph)) * 2.5f * s;
                float x0 = x + fx * along, y0 = y + fy * along;
                float reach = (11.5f - 1.5f * (un < 0 ? -un : un)) * s;
                float ex = x0 + px * side * reach + fx * (un * 4.f * s + stride);
                float ey = y0 + py * side * reach + fy * (un * 4.f * s + stride);
                float kx = x0 + px * side * reach * .55f + fx * (un * 1.5f * s + stride * .5f);
                float ky = y0 + py * side * reach * .55f + fy * (un * 1.5f * s + stride * .5f);
                NkVec2f A = cam.Project(x0, y0, zb - 1.5f * s), K = cam.Project(kx, ky, zb + 1.5f * s + lf), F = cam.Project(ex, ey, lf);
                float dp = cam.Depth(0, (y0 + ky + ey) / 3.f, zb - 2.f * s);    // côté lointain derrière, proche devant
                q.Taper(dp, A, K, 3.4f * s, 2.8f * s, leg);
                q.Taper(dp, K, F, 2.8f * s, 1.2f * s, leg);
                q.Ellipse(dp, F, 1.1f * s, 1.1f * s * cam.tilt + .2f * s, color.Shade(0.7f));
            }

        // Antennes = grosses "oreilles" effilées avec reflet, posées sur le dessus de la tête (toujours devant la tête)
        float sway = NkSin(t_ * 5) * 0.8f * s;
        for (int side = -1; side <= 1; side += 2) {
            float bx = hx + px * side * 4.2f * s, by = hy + py * side * 4.2f * s, bz = hZ + 6.0f * s;
            float tx = bx + (fx * 2.0f + px * side * 5.0f) * s + px * side * sway, ty = by + (fy * 2.0f + py * side * 5.0f) * s + py * side * sway;
            NkVec2f B = cam.Project(bx, by, bz), T = cam.Project(tx, ty, bz + 5.5f * s);
            float dp = headDepth + hR * 1.1f;
            q.Taper(dp, B, T, 3.6f * s, 2.0f * s, leg);
            q.Ellipse(dp, T, 1.0f * s, 1.0f * s, leg);
            q.Taper(dp + 0.01f, B, T, 1.5f * s, 0.8f * s, hi);              // reflet intérieur
        }

        // Yeux : grands, blancs, pupille vers l'avant, paupière sombre inclinée (regard déterminé), toujours visibles
        NkVec2f hc = cam.Project(hx, hy, hZ);
        for (int side = -1; side <= 1; side += 2) {
            NkVec2f e = cam.Project(hx + (fx * 4.4f + px * side * 4.6f) * s, hy + (fy * 4.4f + py * side * 4.6f) * s, hZ + 5.2f * s);
            NkVec2f pu = cam.Project(hx + (fx * 5.6f + px * side * 4.6f) * s, hy + (fy * 5.6f + py * side * 4.6f) * s, hZ + 5.2f * s);
            float dir = hc.x > e.x ? 1.f : -1.f;                             // vers le centre de la tête (écran)
            float dp = headDepth + hR * 0.95f;
            q.Ellipse(dp, e, 2.7f * s, 2.9f * s, NkColor(255, 255, 255, 255));
            q.Ellipse(dp + 0.01f, NkVec2f(e.x + (pu.x - e.x) * .7f, e.y + (pu.y - e.y) * .7f + .3f * s), 1.4f * s, 1.6f * s, NkColor(25, 15, 20, 255));
            q.Line(dp + 0.02f, NkVec2f(e.x + dir * 3.f * s, e.y - 1.9f * s), NkVec2f(e.x - dir * 3.f * s, e.y - 3.0f * s), 1.3f * s, NkColor(55, 15, 30, 255));
        }
    }

private:
    NkLcg32 rng_;
    float born_ = 0;                               // âge (secondes) : apparition en douceur
    float tx_ = 0, ty_ = 0, phase_ = 0, t_ = 0, dropX_ = 0, dropY_ = 0, avX_ = 0, avY_ = 0, avT_ = 0;
    NkVec2f wp_[160]; int nwp_ = 0, iwp_ = 0;
    float stuckAcc_ = 0, sx_ = 0, sy_ = 0, esc_ = 0, escSign_ = 1; int stuck_ = 0;
    bool  hasTarget_ = false; int state_ = 0;      // 0 libre, 1 va chercher, 2 porte
    Box*  box_ = nullptr;
};

// =============================================================================
//  SCENE : update, collisions, z-order global par pièce (tri fusion stable)
// =============================================================================
// La scène POSSÈDE ses entités : un NkVector<NkUniquePtr<Entity>>. Chaque entité est créée par NkMakeUnique<T>()
// puis rangée sous son type de base (conversion NkUniquePtr<T> -> NkUniquePtr<Entity>, qui exige un destructeur
// virtuel : voir Entity). La destruction est automatique, et unique, à la sortie de la scène ou au retrait des
// entités mortes.
class Scene {
public:
    NkView25D camera;

    template <class T> T* Add() {
        NkUniquePtr<T> p = NkMakeUnique<T>();
        T* raw = p.Get();
        items_.PushBack(NkUniquePtr<Entity>(static_cast<NkUniquePtr<T>&&>(p)));
        return raw;
    }

    // Vide la scène (changement de niveau) : les entités sont détruites par leurs pointeurs uniques.
    void Clear() { items_.Clear(); cmds_.Clear(); }

    void Update(float dt) {
        for (usize i = 0; i < items_.Size(); ++i) items_[i]->Update(dt);
        Resolve();
        for (usize i = 0; i < items_.Size(); ++i) items_[i]->LateUpdate();
        RemoveDead();
    }

    void Render(Dl& dl) {
        for (usize i = 0; i < items_.Size(); ++i) items_[i]->DrawShadow(dl, camera);
        cmds_.Clear();
        RenderQueue q{cmds_};
        for (usize i = 0; i < items_.Size(); ++i) items_[i]->Emit(q, camera);
        // Tri fusion STABLE par profondeur croissante : du plus loin au plus près.
        NkStableSort(cmds_.Data(), cmds_.Data() + cmds_.Size(), [](const DrawCmd& a, const DrawCmd& b) { return a.depth < b.depth; });
        // Project rend des PIXELS : les positions suivent la vue toutes seules, mais les TAILLES (épaisseurs, rayons) sont
        // en unités de plan ; on les multiplie par l'échelle de la vue pour que tout grandisse ensemble.
        const float k = camera.Scale();
        for (usize i = 0; i < cmds_.Size(); ++i) {
            const DrawCmd& c = cmds_[i];
            switch (c.kind) {
                case CMD_LINE:    dl.AddLine(c.p0, c.p1, c.col, c.a * k); break;
                case CMD_TAPER:   dl.AddTaperedLine(c.p0, c.p1, c.col, c.a * k, c.b * k); break;
                case CMD_ELLIPSE: dl.AddEllipseFilled(c.p0, c.a * k, c.b * k, c.col); break;
                case CMD_SPHERE:  DrawSphere(dl, camera, c.p0.x, c.p0.y, c.a, c.b * k, c.col); break;
                case CMD_CUSTOM:  static_cast<const ICustomDraw*>(c.ptr)->DrawPiece(dl, camera, (int)c.a); break;
                case CMD_BOX:     static_cast<const Box*>(c.ptr)->DrawBody(dl, camera); break;
            }
        }
    }

private:
    void RemoveDead() {
        usize dead = 0;
        for (usize i = 0; i < items_.Size(); ++i) if (items_[i]->dead) ++dead;
        if (!dead) return;
        NkVector<NkUniquePtr<Entity>> live;
        for (usize i = 0; i < items_.Size(); ++i) {
            if (!items_[i]->dead) live.PushBack(static_cast<NkUniquePtr<Entity>&&>(items_[i]));
        }                                                                   // les morts sont détruits avec items_
        items_ = static_cast<NkVector<NkUniquePtr<Entity>>&&>(live);
    }
    void Resolve() {
        usize n = items_.Size();
        for (usize i = 0; i < n; ++i) {
            Entity* a = items_[i].Get(); if (a->kind != KIND_ANT) continue;
            Ant* A = static_cast<Ant*>(a);
            for (usize j = 0; j < n; ++j) {                                   // obstacles génériques (plateau)
                Entity* o = items_[j].Get();
                if (o->kind == KIND_BOARD) { float nx, ny; if (o->ResolveCircle(A->x, A->y, A->Radius(), nx, ny)) A->Nudge(nx, ny); }
            }
            for (usize j = 0; j < n; ++j) {
                Entity* b = items_[j].Get(); if (b == a || b->kind > KIND_BOX) continue;
                if (b->kind == KIND_ANT) {                                     // fourmi / fourmi (traité une fois)
                    if (j < i) continue;
                    Ant* B = static_cast<Ant*>(b);
                    if (A->ghost > 0 || B->ghost > 0) continue;
                    float dx = B->x - A->x, dy = B->y - A->y, d = NkVec2f(dx, dy).Len(), md = A->Radius() + B->Radius();
                    if (d < md && d > 1e-4f) {
                        float nx = dx / d, ny = dy / d, pu = (md - d) * .5f;
                        A->x -= nx * pu; A->y -= ny * pu; B->x += nx * pu; B->y += ny * pu;
                        A->Nudge(-nx, -ny); B->Nudge(nx, ny);
                    }
                } else {                                                       // fourmi / boîte (non portée)
                    Box* B = static_cast<Box*>(b); if (B->carried) continue;
                    float cx, cy; B->Closest(A->x, A->y, cx, cy);
                    float dx = A->x - cx, dy = A->y - cy, d = NkVec2f(dx, dy).Len(), r = A->Radius();
                    if (d < r) {
                        float nx, ny;
                        if (d > 1e-4f) { nx = dx / d; ny = dy / d; }
                        else { float ex = A->x - B->x, ey = A->y - B->y, el = NkMax(1e-4f, NkVec2f(ex, ey).Len()); nx = ex / el; ny = ey / el; }
                        A->x += nx * (r - d); A->y += ny * (r - d); A->Nudge(nx, ny);
                    }
                }
            }
        }
    }
    NkVector<NkUniquePtr<Entity>> items_;
    NkVector<DrawCmd> cmds_;
};

} // namespace nk25d
