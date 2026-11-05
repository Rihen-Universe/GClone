// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Nk25D.h : Fourmis + boîtes 2.5D procédurales, collisions, portage de blocs.
// Migré le 04/11/2026 sur Nkentseu : plus aucune bibliothèque standard, plus de couche maison pour ce que le
// moteur fournit déjà (NkVec2f, NkColor, NkSin..., NkVector, NkUniquePtr, NkStableSort, NkConvexHull2D, NkLcg32,
// NkGuiDrawList). Reste ici ce qui est propre au jeu : la caméra 2.5D, les entités, la file de rendu.
//
// RÔLE DE CE FICHIER : c'est le socle du jeu, celui que tous les autres incluent. Il définit, de bas en haut :
//   1. les outils de texte (GameText, GameTextOutline) et de volume (DrawSphere) ;
//   2. la FILE DE RENDU (RenderQueue) : chaque objet « émet » des commandes de dessin avec une profondeur, la scène les
//      trie, puis les dessine (algorithme du peintre) ;
//   3. les ENTITÉS : Entity (la base), Box (un bloc que la fourmi porte), Ant (la fourmi) ;
//   4. la SCÈNE (Scene) : elle possède les entités, les met à jour, résout les collisions et dessine.
// Articulation : NkView25D.h fournit la vue (projection, profondeur, ombre) ; Nk25DArt.h dessine l'habillage et inclut
// ce fichier ; Nk25DBoard.h définit le plateau, qui est lui aussi une Entity ; Nk25DGame.h pilote la partie ; main.cpp
// relie le tout au moteur.
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

// Raccourci : la liste de dessin du moteur (là où on accumule les sommets à afficher).
using Dl = nkgui::NkGuiDrawList;

// Police du jeu : posée une fois par l'application (les polices embarquées de NKFont, chargées par NkCanvasGuiApp).
// Le texte est un détail d'interface : le passer en paramètre de chaque primitive alourdirait toutes les signatures.
// Rend une RÉFÉRENCE sur le pointeur, pour qu'on puisse à la fois le lire et l'écrire (main.cpp : GameFont() = FontBody()).
inline nkgui::NkGuiFont*& GameFont()
{
    static nkgui::NkGuiFont* f = nullptr;
    return f;
}

// =============================================================================
//  VUE 2.5D : NkView25D (NkView25D.h), qui hérite de renderer::NkView2D
// =============================================================================
// La caméra 2.5D (projection oblique, hauteur z, profondeur de tri, ombre portée) et son modèle mathématique complet
// vivent dans NkView25D.h, écrit SANS dépendance au jeu pour monter tel quel dans NKCanvas. Elle remplace la structure
// Camera du 04/11/2026 (Rodolf, 05/11/2026) : `center` et `size`, hérités de NkView2D, remplacent `origin` et portent
// l'adaptation à l'écran. Ci-dessous, l'ANCIENNE formule, gardée (« on ne supprime pas ») pour le banc : avec une vue
// neutre, NkView25D::Project doit rendre exactement les mêmes points qu'elle (voir VerifierVue dans main.cpp).
struct CameraAncienne {
    // Décalage du monde (translation ajoutée à la projection). Nul par défaut.
    NkVec2f origin{0.f, 0.f};
    // tan(phi) : 0.65 correspond à phi = 33 degrés.
    float tilt = 0.65f;
    // Direction de la lumière, par unité de hauteur.
    NkVec2f lightDir{0.55f, 0.30f};

    // Monde -> écran selon l'ancienne formule : (origin.x + x, origin.y + y * tilt - z).
    NkVec2f Project(float x, float y, float z) const
    {
        return NkVec2f(origin.x + x, origin.y + y * tilt - z);
    }

    // Position de l'ombre : le point décalé par la lumière, projeté au sol (z = 0).
    NkVec2f ShadowPos(float x, float y, float z) const
    {
        return Project(x + lightDir.x * z, y + lightDir.y * z, 0.f);
    }

    // Profondeur de tri : y + z * tilt (le premier paramètre est ignoré).
    float Depth(float, float y, float z) const
    {
        return y + z * tilt;
    }
};

// Texte centré en (cx, cy), de hauteur `size` (pixels), avec une ombre portée optionnelle.
//   dl : la liste de dessin.   cx, cy : le centre du texte (pixels).   size : la hauteur du texte (pixels).
//   t : le texte.   c : sa couleur.   shadow : vrai pour dessiner d'abord une ombre noire légère décalée.
// Sans police (avant l'initialisation de l'application), la fonction ne fait rien.
inline void GameText(Dl& dl, float cx, float cy, float size, const char* t, const NkColor& c, bool shadow = true)
{
    if (!GameFont())
    {
        return;
    }
    if (shadow)
    {
        // L'ombre : noir à alpha 90, décalée de 5 % de la taille à droite et de 8 % vers le bas.
        renderer::NkTexteCentreTaille(dl, GameFont(), cx + size * .05f, cy - size * .5f + size * .08f, t, NkColor(0, 0, 0, 90), size);
    }
    // Le texte : le point d'ancrage est le haut du texte, d'où le retrait d'une demi-hauteur pour centrer en y.
    renderer::NkTexteCentreTaille(dl, GameFont(), cx, cy - size * .5f, t, c, size);
}

// Texte avec CONTOUR (les nombres de l'interface) : le même texte, dans la couleur du contour, est posé huit fois
// autour de la position (aux huit directions, à `epaisseur` pixels), puis le texte de la couleur voulue par-dessus.
// C'est le « dilatation morphologique » le plus simple : le contour est l'union des huit copies décalées. Huit
// décalages suffisent tant que l'épaisseur reste petite devant la taille du texte (ici 7 % de la hauteur).
//   dl : la liste de dessin.   cx, cy : le centre.   size : la hauteur du texte (pixels).   t : le texte.
//   c : la couleur du texte.   outline : la couleur du contour.
inline void GameTextOutline(Dl& dl, float cx, float cy, float size, const char* t, const NkColor& c, const NkColor& outline)
{
    if (!GameFont())
    {
        return;
    }
    // Contour épais (10 % de la hauteur) puis texte ÉPAISSI : la police embarquée n'a pas de graisse, on la fabrique en posant
    // le texte quatre fois, décalé de 1.8 % de la hauteur (Rodolf, 05/11 : « nombres trop fins »).
    // o : l'épaisseur du contour (au moins 1,4 pixel) ; b : le décalage de l'épaississement du texte.
    const float o = NkMax(1.4f, size * 0.10f);
    const float b = size * 0.018f;
    // Huit directions : gauche, droite, haut, bas, puis les quatre diagonales (0,7 environ = 1 / racine de 2).
    static const float dx[8] = {-1, 1, 0, 0, -.7f, .7f, -.7f, .7f};
    static const float dy[8] = {0, 0, -1, 1, -.7f, -.7f, .7f, .7f};
    // 1. Le contour : huit copies de la couleur du contour.
    for (int i = 0; i < 8; ++i)
    {
        renderer::NkTexteCentreTaille(dl, GameFont(), cx + dx[i] * o, cy - size * .5f + dy[i] * o, t, outline, size);
    }
    // 2. L'épaississement : quatre copies de la couleur du texte (gauche, droite, haut, bas), décalées de b.
    for (int i = 0; i < 4; ++i)
    {
        renderer::NkTexteCentreTaille(dl, GameFont(), cx + dx[i] * b, cy - size * .5f + dy[i] * b, t, c, size);
    }
    // 3. Le texte lui-même, au centre.
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
//   dl : la liste de dessin.   cam : la vue.   x, y, z : le centre de la sphère dans le monde.
//   r : le rayon, en PIXELS (la file de rendu l'a déjà multiplié par l'échelle de la vue).   base : la couleur.
inline void DrawSphere(Dl& dl, const NkView25D& cam, float x, float y, float z, float r, const NkColor& base)
{
    NkVec2f c = cam.Project(x, y, z);
    // Le contour : un disque de rayon r + e (e = 11 % de r, au moins 0,7 pixel), très sombre (55 %).
    dl.AddEllipseFilled(c, r + NkMax(0.7f, r * .11f), r + NkMax(0.7f, r * .11f), base.Shade(0.55f));
    // La zone d'ombre propre : un disque de rayon r à 78 % de la teinte.
    dl.AddEllipseFilled(c, r, r, base.Shade(0.78f));
    // La partie éclairée : un disque de 88 % du rayon, décalé vers le haut gauche (vers la lumière).
    dl.AddEllipseFilled(NkVec2f(c.x - r * .10f, c.y - r * .12f), r * .88f, r * .88f, base);
    // Le reflet : une petite ellipse mélangée à 45 % de blanc, plus haut et plus à gauche encore.
    dl.AddEllipseFilled(NkVec2f(c.x - r * .30f, c.y - r * .35f), r * .40f, r * .32f, base.Lerp(NkColor(255, 255, 255, 255), 0.45f));
}

// =============================================================================
//  FILE DE RENDU : des commandes simples, triées par profondeur
// =============================================================================
// Chaque entité « émet » ses morceaux dessinables (sphères, pattes, yeux...) avec leur profondeur ; la scène
// trie ensuite TOUTES les commandes ensemble et les dessine dans l'ordre. C'est l'algorithme du peintre à
// l'échelle des morceaux et non des objets : la tête d'une fourmi peut passer devant le corps d'une autre tout
// en restant derrière sa propre antenne. Le tri est STABLE (NkStableSort, tri fusion) : deux commandes de même
// profondeur gardent leur ordre d'émission, donc l'image ne scintille pas d'une trame à l'autre.

// Interface d'un objet qui sait dessiner lui-même un de ses morceaux (le plateau dessine ses cubes ainsi).
// DrawPiece est appelée au moment où la commande CMD_CUSTOM correspondante arrive dans l'ordre trié.
struct ICustomDraw {
    // Dessine le morceau d'indice idx (le sens de idx appartient à l'objet : pour le plateau, c'est la case).
    virtual void DrawPiece(Dl& dl, const NkView25D& cam, int idx) const = 0;

    // Destructeur virtuel : on détruit toujours à travers un pointeur sur la base.
    virtual ~ICustomDraw()
    {
    }
};

// Les sortes de commandes de dessin.
enum CmdKind {
    CMD_LINE,       // un trait d'épaisseur constante
    CMD_ELLIPSE,    // une ellipse pleine
    CMD_SPHERE,     // une sphère (trois disques, voir DrawSphere)
    CMD_BOX,        // un bloc complet (Box::DrawBody)
    CMD_TAPER,      // un trait dont l'épaisseur varie d'un bout à l'autre (une patte, une antenne)
    CMD_CUSTOM      // un morceau dessiné par un objet (ICustomDraw)
};

// Une commande de dessin. Les champs servent différemment selon `kind` : voir les fabriques de RenderQueue.
struct DrawCmd {
    // Profondeur de tri (voir NkView25D::Depth) : plus grande = plus près = dessinée plus tard.
    float depth;
    // La sorte de commande (valeur de CmdKind).
    int kind;
    // Deux points (écran) pour les traits, le centre pour une ellipse.
    NkVec2f p0;
    NkVec2f p1;
    // Deux nombres : épaisseurs, rayons, ou hauteur et rayon pour une sphère.
    float a;
    float b;
    // La couleur.
    NkColor col;
    // Un pointeur vers l'objet à dessiner (commandes CMD_BOX et CMD_CUSTOM), sinon nul.
    const void* ptr;
};

// La file : un vecteur de commandes (appartient à la scène) et des fabriques qui le remplissent.
struct RenderQueue {
    NkVector<DrawCmd>& cmds;

    // Ajoute une commande déjà construite.
    void Push(const DrawCmd& c)
    {
        cmds.PushBack(c);
    }

    // Un trait de a à b, d'épaisseur w (unités de plan). dp : la profondeur.
    void Line(float dp, NkVec2f a, NkVec2f b, float w, NkColor c)
    {
        Push({dp, CMD_LINE, a, b, w, 0, c, nullptr});
    }

    // Un trait effilé de a à b, d'épaisseur w0 au départ et w1 à l'arrivée.
    void Taper(float dp, NkVec2f a, NkVec2f b, float w0, float w1, NkColor c)
    {
        Push({dp, CMD_TAPER, a, b, w0, w1, c, nullptr});
    }

    // Une ellipse pleine de centre c0 (écran) et de rayons rx et ry.
    void Ellipse(float dp, NkVec2f c0, float rx, float ry, NkColor c)
    {
        Push({dp, CMD_ELLIPSE, c0, NkVec2f(), rx, ry, c, nullptr});
    }

    // Une sphère de centre (x, y, z) dans le MONDE (la projection se fait au dessin) et de rayon r.
    void Sphere(float dp, float x, float y, float z, float r, NkColor c)
    {
        Push({dp, CMD_SPHERE, NkVec2f(x, y), NkVec2f(), z, r, c, nullptr});
    }

    // Un morceau dessiné par l'objet `iface` (un ICustomDraw), qui reçoit l'indice idx (rangé dans le champ a).
    void Custom(float dp, const void* iface, int idx)
    {
        Push({dp, CMD_CUSTOM, NkVec2f(), NkVec2f(), (float)idx, 0, NkColor(0, 0, 0, 0), iface});
    }

    // Un bloc complet : `b` pointe sur le Box.
    void BoxCmd(float dp, const void* b)
    {
        Push({dp, CMD_BOX, NkVec2f(), NkVec2f(), 0, 0, NkColor(0, 0, 0, 0), b});
    }
};

// Les sortes d'entités : la scène s'en sert pour savoir à qui elle a affaire sans passer par un typage dynamique.
// L'ORDRE compte : Scene::Resolve teste `kind > KIND_BOX` pour ignorer le plateau et le nid.
enum EntityKind { KIND_ANT, KIND_BOX, KIND_BOARD, KIND_HOLE };

// La base de tout ce que la scène contient : une position, une sorte, un drapeau « mort », et cinq opérations.
struct Entity {
    // Position dans le monde (unités de plan).
    float x = 0;
    float y = 0;
    // La sorte (valeur de EntityKind), fixée par le constructeur de chaque classe.
    int kind = 0;
    // Vrai quand l'entité doit disparaître : la scène la retire à la fin de la mise à jour.
    bool dead = false;

    virtual ~Entity()
    {
    }

    // Obstacle : repousse un cercle hors de l'entité ; renvoie true + normale si contact
    // Par défaut, une entité n'est pas un obstacle (seul le plateau redéfinit cette fonction).
    virtual bool ResolveCircle(float&, float&, float, float&, float&)
    {
        return false;
    }

    // Mise à jour à chaque image. dt : le temps écoulé, en secondes.
    virtual void Update(float dt) = 0;

    // Seconde mise à jour, après les collisions (par exemple, le bloc suit la fourmi qui le porte une fois la fourmi replacée).
    virtual void LateUpdate()
    {
    }

    // Dessine l'ombre au sol. Appelée pour toutes les entités AVANT le dessin trié des corps.
    virtual void DrawShadow(Dl&, const NkView25D&) const = 0;

    // Ajoute les morceaux dessinables de l'entité à la file de rendu.
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
    // Dimensions : largeur w (x), profondeur d (y), hauteur h (z), et le cap yaw en radians.
    float w = 56;
    float d = 44;
    float h = 22;
    float yaw = 0;
    // Le nombre affiché sur le dessus du bloc.
    int value = 0;
    // La couleur du bloc (le dessus est éclairci, les côtés assombris).
    NkColor color{80, 200, 240, 255};
    // mystery : le texte est « ? » ; faceMotion : le bloc s'oriente dans sa direction de déplacement ;
    // showText : dessiner le nombre.
    bool mystery = false;
    bool faceMotion = false;
    bool showText = true;
    // Échelle du bloc quand il est porté (le bloc rétrécit à 38 % par défaut ; le jeu règle 80 %).
    float carryScale = 0.38f;
    // Hauteur ajoutée (portage) : le bloc se lève au-dessus de la fourmi qui le porte.
    float lift = 0;
    // Vrai quand une fourmi porte le bloc, et laquelle.
    bool carried = false;
    Ant* carrier = nullptr;

    // Constructeur : marque l'entité comme boîte.
    Box()
    {
        kind = KIND_BOX;
    }

    // Écrase le bloc d'un coup (effet d'appui) : donne une vitesse au ressort d'écrasement. s : la force.
    void Press(float s = 8)
    {
        vel_ -= s * 20;
    }

    // Demande au bloc d'aller se placer en (tx, ty) (centre), en glissant en douceur.
    void MoveTo(float tx, float ty)
    {
        tx_ = tx;
        ty_ = ty;
        moving_ = true;
    }

    // Demande au bloc de tourner doucement vers l'angle a (radians).
    void RotateTo(float a)
    {
        yawT_ = a;
    }

    // Fixe le cap tout de suite, sans transition.
    void SetYaw(float a)
    {
        yawT_ = a;
        yaw = yawT_;
    }

    // Passe le bloc à l'état porté (c vrai) ou posé (c faux).
    // Porté : il vise l'échelle carryScale, arrête son déplacement propre et son saut.
    // Posé : il oublie sa porteuse et redescend (lift = 0).
    void SetCarried(bool c)
    {
        carried = c;
        scT_ = c ? carryScale : 1.f;
        if (c)
        {
            moving_ = false;
            hop_ = 0;
        }
        else
        {
            carrier = nullptr;
            lift = 0;
        }
    }

    // L'échelle actuelle du bloc (1 posé, carryScale porté, avec une transition).
    float Scale() const
    {
        return sc_;
    }

    // Point de l'empreinte le plus proche de (px,py) (pour collisions / ramassage) : on exprime le point dans le
    // repère local du pavé (rotation inverse), on le borne au rectangle, puis on revient au repère monde.
    //   px, py : le point.   cx, cy : sorties, le point de l'empreinte le plus proche (monde).
    void Closest(float px, float py, float& cx, float& cy) const
    {
        float cs = NkCos(yaw);
        float sn = NkSin(yaw);
        float dx = px - x;
        float dy = py - y;
        // (lx, ly) : le point dans le repère du pavé (rotation de -yaw).
        float lx = dx * cs + dy * sn;
        float ly = -dx * sn + dy * cs;
        // Demi-dimensions de l'empreinte, tenant compte de l'échelle actuelle.
        float hx = w * sc_ * .5f;
        float hy = d * sc_ * .5f;
        float qx = NkClamp(lx, -hx, hx);
        float qy = NkClamp(ly, -hy, hy);
        // Retour au repère monde (rotation de +yaw).
        cx = x + qx * cs - qy * sn;
        cy = y + qx * sn + qy * cs;
    }

    // Mise à jour à chaque image : échelle, déplacement, cap, ressort d'écrasement. dt : secondes.
    void Update(float dt) override
    {
        // Lissage exponentiel : v += (cible - v) * (1 - e^(-k dt)). Indépendant du pas de temps (contrairement à
        // v += (cible - v) * k * dt qui diverge si k dt > 1), et sans dépassement.
        sc_ += (scT_ - sc_) * (1 - NkExp(-10 * dt));
        if (!carried)
        {
            if (moving_)
            {
                // dist : distance restante à la cible ; k : la part de chemin parcourue cette image (constante de temps 12).
                float dx = tx_ - x;
                float dy = ty_ - y;
                float dist = NkVec2f(dx, dy).Len();
                float k = 1 - NkExp(-12 * dt);
                x += dx * k;
                y += dy * k;
                // Petit saut pendant le déplacement : 25 % de la distance, au plus 14 unités de haut.
                hop_ = NkMin(dist * 0.25f, 14.f);
                // Le bloc se tourne dans sa direction de marche, sauf s'il est presque arrivé (moins de 6 unités).
                if (faceMotion && dist > 6)
                {
                    yawT_ = NkAtan2(dy, dx);
                }
                // Arrivé (moins de 0,3 unité) : on se place exactement sur la cible et on arrête.
                if (dist < .3f)
                {
                    x = tx_;
                    y = ty_;
                    moving_ = false;
                    hop_ = 0;
                }
            }
            // Le cap tend vers le cap visé par le chemin angulaire le plus court (constante de temps 14).
            yaw += NkAngle::FromRad(yawT_ - yaw).Rad() * (1 - NkExp(-14 * dt));
        }
        // Ressort amorti (écrasement à l'appui) : accélération = -k x - c v, intégrée à pas explicite.
        // k = 180 (raideur), c = 14 (amortissement).
        vel_ += (-180 * squash_ - 14 * vel_) * dt;
        squash_ += vel_ * dt;
    }

    // Ombre : enveloppe convexe de la base et du dessus décalé par la lumière (NkConvexHull2D).
    void DrawShadow(Dl& dl, const NkView25D& cam) const override
    {
        // top : la hauteur du dessus du bloc (taille, saut et portage).
        float top = h * sc_ + hop_ + lift;
        NkVector<NkVec2f> pts;
        NkVector<NkVec2f> hull;
        for (int i = 0; i < 4; ++i)
        {
            NkVec2f c = Corner(i);
            // Chaque coin donne deux points : lui-même au sol, et l'ombre de son sommet.
            pts.PushBack(cam.Project(c.x, c.y, 0));
            pts.PushBack(cam.ShadowPos(c.x, c.y, top));
        }
        NkConvexHull2D(pts, hull);
        // Un bloc porté projette une ombre plus pâle (alpha 35) qu'un bloc posé (55).
        if (hull.Size() >= 3)
        {
            dl.AddConvexPolyFilled(hull.Data(), (int32)hull.Size(), NkColor(0, 0, 0, carried ? 35 : 55));
        }
    }

    // Ajoute le bloc à la file de rendu. La profondeur est prise au CENTRE du bloc, à la hauteur de son dessus
    // (au moins 4 unités, pour qu'un bloc très écrasé ne passe pas derrière le sol).
    void Emit(RenderQueue& q, const NkView25D& cam) const override
    {
        float hh = NkMax(4.f, h * sc_);
        q.BoxCmd(cam.Depth(x, y, hop_ + lift + hh), this);
    }

    // Dessine le bloc : les faces latérales tournées vers la caméra, puis le dessus, puis le nombre.
    //   dl : la liste de dessin.   cam : la vue.
    void DrawBody(Dl& dl, const NkView25D& cam) const
    {
        // z0 : le bas du bloc ; hh : sa hauteur (écrasée par le ressort : 1,2 % par unité d'écrasement) ; z1 : son dessus.
        float z0 = hop_ + lift;
        float hh = NkMax(4.f * sc_, h * sc_ * (1 - squash_ * 0.012f));
        float z1 = z0 + hh;
        NkVec2f c[4];
        for (int i = 0; i < 4; ++i)
        {
            c[i] = Corner(i);
        }
        // lv : le vecteur UNITAIRE vers la lumière (l'opposé de la direction de la lumière).
        NkVec2f lv(-cam.lightDir.x, -cam.lightDir.y);
        lv = lv / lv.Len();
        for (int i = 0; i < 4; ++i)
        {
            NkVec2f a = c[i];
            NkVec2f b = c[(i + 1) & 3];
            // nv : la normale sortante unitaire du côté a -> b.
            NkVec2f nv(b.y - a.y, -(b.x - a.x));
            nv = nv / nv.Len();
            // Face tournée vers le fond : cachée.
            if (nv.y <= 0.001f)
            {
                continue;
            }
            // Éclairage de la face : 0,5 + 0,35 fois le produit scalaire, borné entre 0,4 et 0,95.
            float lit = NkClamp(0.5f + 0.35f * (nv.x * lv.x + nv.y * lv.y), 0.4f, 0.95f);
            NkVec2f q[4] = {
                cam.Project(a.x, a.y, z0),
                cam.Project(b.x, b.y, z0),
                cam.Project(b.x, b.y, z1),
                cam.Project(a.x, a.y, z1)
            };
            dl.AddConvexPolyFilled(q, 4, color.Shade(lit));
            // Arête haute de la face : un liseré clair (35 % de blanc) épais de 1,5 unité (passée en pixels).
            dl.AddLine(q[3], q[2], color.Lerp(NkColor(255, 255, 255, 255), 0.35f), 1.5f * sc_ * cam.Scale());
        }
        // Le dessus : T est le contour, I un rectangle intérieur rentré de 8 unités (biseau, k est le facteur de rétrécissement).
        NkVec2f T[4];
        NkVec2f I[4];
        float k = 1 - 8.f / NkMin(w, d);
        for (int i = 0; i < 4; ++i)
        {
            T[i] = cam.Project(c[i].x, c[i].y, z1);
            I[i] = cam.Project(x + (c[i].x - x) * k, y + (c[i].y - y) * k, z1);
        }
        // Dessus éclairci de 22 %, puis le biseau de 10 %.
        dl.AddConvexPolyFilled(T, 4, color.Lerp(NkColor(255, 255, 255, 255), 0.22f));
        dl.AddConvexPolyFilled(I, 4, color.Lerp(NkColor(255, 255, 255, 255), 0.10f));
        // Le nombre, seulement si le bloc est assez grand (échelle > 0,6) : un bloc porté est trop petit pour le lire.
        if (showText && sc_ > 0.6f)
        {
            NkString s = mystery ? NkString("?") : NkFormat("{0}", value);
            NkVec2f t = cam.Project(x, y, z1);
            // Ombre noire d'abord (décalée d'un pixel à droite et de deux vers le bas), puis le texte blanc.
            GameText(dl, t.x + 1, t.y + 2, 22 * sc_ * cam.Scale(), s.CStr(), NkColor(0, 0, 0, 90), false);
            GameText(dl, t.x, t.y, 22 * sc_ * cam.Scale(), s.CStr(), NkColor(255, 255, 255, 255), false);
        }
    }

private:
    // Le coin i (0 à 3) de l'empreinte au sol, dans le monde : on tourne le coin local de yaw puis on translate.
    // Ordre des coins : haut gauche, haut droit, bas droit, bas gauche.
    NkVec2f Corner(int i) const
    {
        static const float sx[4] = {-1, 1, 1, -1};
        static const float sy[4] = {-1, -1, 1, 1};
        // (lx, ly) : le coin dans le repère du pavé ; (cs, sn) : cosinus et sinus du cap.
        float lx = sx[i] * w * sc_ * .5f;
        float ly = sy[i] * d * sc_ * .5f;
        float cs = NkCos(yaw);
        float sn = NkSin(yaw);
        return NkVec2f(x + lx * cs - ly * sn, y + lx * sn + ly * cs);
    }

    // État interne : (tx_, ty_) la cible de MoveTo ; hop_ le saut en cours ; squash_ et vel_ le ressort d'écrasement ;
    // yawT_ le cap visé ; sc_ l'échelle actuelle et scT_ l'échelle visée.
    float tx_ = 0;
    float ty_ = 0;
    float hop_ = 0;
    float squash_ = 0;
    float vel_ = 0;
    float yawT_ = 0;
    float sc_ = 1;
    float scT_ = 1;
    // Vrai pendant un MoveTo.
    bool moving_ = false;
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
    // La couleur de la fourmi (celle de son panier et des cubes qu'elle prend).
    NkColor color{225, 40, 60, 255};
    // speed : vitesse en unités par seconde ; heading : le cap en radians ; scale : la taille relative.
    float speed = 70;
    float heading = 0;
    float scale = 1.0f;
    // Jeu : l'indice de couleur de la fourmi, la cellule visée (-1 = aucune), et un champ libre `owner`.
    int colorIdx = -1;
    int task = -1;
    int owner = -1;
    // Distance à laquelle la cible est considérée atteinte.
    float arriveRadius = 3;
    // Détruit le bloc porté à l'arrivée (jeu).
    bool eatOnArrive = false;
    // 3 = fourmi (6 pattes) ; 4 = 8 pattes.
    int legPairs = 3;

    // Graine : l'adresse de l'objet (chaque fourmi a sa suite, rejouable avec Seed)
    Ant()
    {
        kind = KIND_ANT;
        rng_.Seed((uint32)((usize)this >> 4) * 2654435761u | 1u);
    }

    // Le rayon de collision de la fourmi : 8 unités à l'échelle 1.
    float Radius() const
    {
        return 8.f * scale;
    }

    // Donne une cible simple (sans chemin) : la fourmi marche tout droit vers (tx, ty). Efface les points de passage.
    void SetTarget(float tx, float ty)
    {
        tx_ = tx;
        ty_ = ty;
        hasTarget_ = true;
        iwp_ = 0;
        nwp_ = iwp_;
    }

    // Chemin : points de passage puis cible finale (calculé par Board::FindPath*)
    //   p, n : les n points de passage (au plus 160 sont gardés).   fx, fy : la cible finale.
    void SetPath(const NkVec2f* p, int n, float fx, float fy)
    {
        nwp_ = n > 160 ? 160 : n;
        for (int i = 0; i < nwp_; ++i)
        {
            wp_[i] = p[i];
        }
        iwp_ = 0;
        tx_ = fx;
        ty_ = fy;
        hasTarget_ = true;
    }

    // Vrai quand la fourmi n'a plus de cible (elle est arrivée).
    bool Arrived() const
    {
        return !hasTarget_;
    }

    // Le nombre de points de passage du chemin en cours.
    int WpCount() const
    {
        return nwp_;
    }

    // L'indice du prochain point de passage.
    int WpIndex() const
    {
        return iwp_;
    }

    // Le point de passage d'indice i.
    NkVec2f Wp(int i) const
    {
        return wp_[i];
    }

    // La cible finale.
    NkVec2f Target() const
    {
        return NkVec2f(tx_, ty_);
    }

    // Vrai si la fourmi est occupée (elle va chercher ou porte un bloc).
    bool Busy() const
    {
        return state_ != 0;
    }

    // Le nombre de blocages constatés depuis le dernier ResetStuck.
    int Stuck() const
    {
        return stuck_;
    }

    // Remet à zéro le compteur de blocages et la manoeuvre de dégagement.
    void ResetStuck()
    {
        stuck_ = 0;
        esc_ = 0;
    }

    // Ramassage direct d'un bloc (jeu) puis transport vers (dx,dy)
    void PickUp(Box* b, float dx, float dy)
    {
        box_ = b;
        b->SetCarried(true);
        b->carrier = this;
        // État 2 : la fourmi porte.
        state_ = 2;
        SetTarget(dx, dy);
        ResetStuck();
    }

    // Le bloc porté, ou nullptr si la fourmi ne porte rien.
    Box* Carried() const
    {
        return state_ == 2 ? box_ : nullptr;
    }

    // Ordre : aller chercher b, puis le déposer vers (dx,dy)
    void Fetch(Box* b, float dx, float dy)
    {
        ResetStuck();
        box_ = b;
        dropX_ = dx;
        dropY_ = dy;
        // État 1 : la fourmi va chercher.
        state_ = 1;
        SetTarget(b->x, b->y);
    }

    // Appelé par Scene lors d'une collision : dévie le long de l'obstacle
    //   nx, ny : la normale de l'obstacle au point de contact (unitaire).
    void Nudge(float nx, float ny)
    {
        // Le côté de contournement se choisit d'après le POINT DE PASSAGE COURANT et non la cible finale : sinon, dans une impasse
        // (cul-de-sac dont la sortie est à l'opposé de la cible), l'évitement pousse la fourmi vers le fond et elle reste coincée.
        const float gx = iwp_ < nwp_ ? wp_[iwp_].x : tx_;
        const float gy = iwp_ < nwp_ ? wp_[iwp_].y : ty_;
        // (tx, ty) : la tangente à l'obstacle (la normale tournée d'un quart de tour) ; (wx, wy) : vers le but.
        float tx = -ny;
        float ty = nx;
        float wx = gx - x;
        float wy = gy - y;
        // On prend la tangente qui va du côté du but (produit scalaire positif).
        if (tx * wx + ty * wy < 0)
        {
            tx = -tx;
            ty = -ty;
        }
        // L'évitement dure 0,4 s ; Update l'ajoute à la direction voulue.
        avX_ = tx;
        avY_ = ty;
        avT_ = 0.4f;
    }

    // Événement « bloc mangé » : levé à l'arrivée au nid, lu puis baissé par le jeu (miettes, rebond du panier).
    bool justAte = false;
    // « Fantôme » (secondes restantes) : une fourmi coincée traverse les autres fourmis (jamais les cubes). Sans cela deux
    // fourmis qui se croisent en sens inverse dans une rainure d'une case se bloquent à jamais (blocage mutuel).
    float ghost = 0;
    // Nombre de blocages constatés (le jeu recale la fourmi au centre d'une case libre au troisième).
    int jams = 0;
    // Nombre de blocs mangés : une fourmi correcte n'en mange qu'un (règle du 05/11).
    int trips = 0;
    // La couleur du dernier bloc mangé (pour les miettes).
    NkColor ateColor{255, 255, 255, 255};

    // Mise à jour à chaque image. dt : secondes.
    // 1. Âge et fantôme ; 2. machine à états du transport ; 3. marche vers la cible (steering) ; 4. LateUpdate.
    void Update(float dt) override
    {
        t_ += dt;
        born_ += dt;
        if (ghost > 0)
        {
            ghost -= dt;
        }
        if (state_ == 1)
        {
            // État 1 : elle va chercher un bloc. Le bloc a disparu ou une autre fourmi l'a pris : retour à l'état libre.
            if (!box_ || box_->carried)
            {
                state_ = 0;
                box_ = nullptr;
                hasTarget_ = false;
            }
            else
            {
                float cx;
                float cy;
                box_->Closest(x, y, cx, cy);
                if (NkVec2f(cx - x, cy - y).Len() <= Radius() + 3.f)
                {
                    // Assez près : elle prend le bloc et part le déposer.
                    box_->SetCarried(true);
                    box_->carrier = this;
                    state_ = 2;
                    SetTarget(dropX_, dropY_);
                    ResetStuck();
                }
                else
                {
                    // Pas encore : le bloc a pu bouger, on remet la cible à jour.
                    SetTarget(box_->x, box_->y);
                }
            }
        }
        else if (state_ == 2 && !hasTarget_)
        {
            // État 2 : elle porte et vient d'arriver à destination.
            if (eatOnArrive)
            {
                // Jeu : le bloc est mangé (il disparaît), on lève l'événement et on compte le voyage.
                justAte = true;
                ++trips;
                ateColor = box_->color;
                box_->SetCarried(false);
                box_->dead = true;
                box_ = nullptr;
                state_ = 0;
                ResetStuck();
            }
            else
            {
                // Sinon : elle dépose le bloc 30 unités devant elle, tourné dans son cap.
                float fx = NkCos(heading);
                float fy = NkSin(heading);
                box_->SetCarried(false);
                box_->x = x + fx * 30 * scale;
                box_->y = y + fy * 30 * scale;
                box_->SetYaw(heading);
                box_ = nullptr;
                state_ = 0;
            }
        }
        if (hasTarget_)
        {
            // Tolérance d'un point de passage (unités de plan).
            const float wpTol = 7.f;
            // Point de passage atteint à wpTol ; le DERNIER est atteint à `arriveRadius` (16 px au nid) : sans cela deux fourmis qui visent le
            // même point final se repoussent (collision) et aucune ne s'approche à 7 px : blocage mutuel à l'entrée du nid (mesuré par le banc).
            while (iwp_ < nwp_
                   && NkVec2f(wp_[iwp_].x - x, wp_[iwp_].y - y).Len() < (iwp_ == nwp_ - 1 ? NkMax(wpTol, arriveRadius) : wpTol))
            {
                ++iwp_;
            }
            // fd : la distance à la cible finale.
            float fd = NkVec2f(tx_ - x, ty_ - y).Len();
            if (iwp_ >= nwp_ && fd < arriveRadius)
            {
                // Arrivée : plus de cible.
                hasTarget_ = false;
                stuckAcc_ = 0;
            }
            else
            {
                // (gx, gy) : le but du moment, le point de passage courant, ou la cible finale s'il n'en reste plus.
                float gx = iwp_ < nwp_ ? wp_[iwp_].x : tx_;
                float gy = iwp_ < nwp_ ? wp_[iwp_].y : ty_;
                // dist : la distance au but, avec un plancher pour ne jamais diviser par zéro.
                float dx = gx - x;
                float dy = gy - y;
                float dist = NkMax(1e-3f, NkVec2f(dx, dy).Len());
                // Détection d'impasse : peu de progrès sur 0.5 s -> manoeuvre de dégagement (latérale + recul)
                stuckAcc_ += dt;
                if (stuckAcc_ >= 0.5f)
                {
                    // Moins de 8 % de la vitesse en 0,5 s : coincée. Elle part en dégagement 0,9 s, d'un côté tiré au hasard.
                    if (esc_ <= 0 && NkVec2f(x - sx_, y - sy_).Len() < speed * 0.08f)
                    {
                        esc_ = 0.9f;
                        escSign_ = rng_.NextUInt32(2) ? 1.f : -1.f;
                        ++stuck_;
                    }
                    // Nouvelle position de référence pour la mesure suivante.
                    sx_ = x;
                    sy_ = y;
                    stuckAcc_ = 0;
                }
                // (vx, vy) : la direction voulue, unitaire.
                float vx = dx / dist;
                float vy = dy / dist;
                if (esc_ > 0)
                {
                    // Dégagement : on part sur le côté (escSign_) avec un petit recul (0,35).
                    esc_ -= dt;
                    float ex = -vy * escSign_ - vx * 0.35f;
                    float ey = vx * escSign_ - vy * 0.35f;
                    vx = ex;
                    vy = ey;
                }
                else if (avT_ > 0)
                {
                    // Évitement : on ajoute la tangente de Nudge, avec une force qui décroît (1,6 au début, 0 à la fin).
                    float k = 1.6f * (avT_ / 0.4f);
                    vx += avX_ * k;
                    vy += avY_ * k;
                    avT_ -= dt;
                }
                // diff : l'écart entre la direction voulue et le cap, ramené entre -PI et PI.
                float diff = NkAngle::FromRad(NkAtan2(vy, vx) - heading).Rad();
                // Le cap tourne vers la direction voulue, au plus 8 radians par seconde.
                heading = NkAngle::FromRad(heading + NkClamp(diff, -8 * dt, 8 * dt)).Rad();
                // La vitesse est réduite quand le cap est mal aligné, avec un plancher de 20 %.
                float v = speed * NkMax(0.2f, NkCos(diff));
                x += NkCos(heading) * v * dt;
                y += NkSin(heading) * v * dt;
                // La phase de marche des pattes avance avec la distance parcourue (0,28 radian par unité).
                phase_ += v * dt * 0.28f;
            }
        }
        else
        {
            stuckAcc_ = 0;
        }
        LateUpdate();
    }

    // Le bloc suit APRÈS les collisions : on le replace juste devant la fourmi et on le soulève de 14 unités.
    void LateUpdate() override
    {
        if (state_ == 2 && box_)
        {
            box_->x = x + NkCos(heading) * scale;
            box_->y = y + NkSin(heading) * scale;
            box_->lift = 14.f * scale;
            box_->SetYaw(heading);
        }
    }

    // Ombre de la fourmi : deux ellipses (abdomen et tête) projetées par la lumière.
    //   dl : la liste de dessin.   cam : la vue.
    void DrawShadow(Dl& dl, const NkView25D& cam) const override
    {
        // s : l'échelle, qui grandit de 25 % à 100 % pendant les 0,3 premières secondes de vie (apparition en douceur).
        float s = scale * (0.25f + 0.75f * NkMin(1.f, born_ / 0.30f));
        float fx = NkCos(heading);
        float fy = NkSin(heading);
        // Position le long du corps (arrière et avant) et rayon de chacune des deux ombres.
        const float off[2] = {-6.5f, 5.5f};
        const float rad[2] = {7.5f, 8.5f};
        for (int i = 0; i < 2; ++i)
        {
            // L'ombre d'un point situé à 6 unités de haut ; l'ellipse est écrasée par l'inclinaison (tilt) et un facteur 1,1.
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
    //   q : la file de rendu.   cam : la vue.
    void Emit(RenderQueue& q, const NkView25D& cam) const override
    {
        // Apparition en douceur : 0 -> 1 en 0.3 s.
        const float born = NkMin(1.f, born_ / 0.30f);
        // Sortie douce (cubique) : l'échelle passe de 25 % à 100 % en ralentissant à la fin.
        const float s = scale * (0.25f + 0.75f * (1.f - (1.f - born) * (1.f - born) * (1.f - born)));
        // (fx, fy) : le vecteur « avant » ; (px, py) : le vecteur « côté » (l'avant tourné d'un quart de tour).
        const float fx = NkCos(heading);
        const float fy = NkSin(heading);
        const float px = -fy;
        const float py = fx;
        // zb : la hauteur du corps, avec un léger rebond de marche (0,5 unité).
        const float zb = 6.f * s + NkSin(phase_ * 2) * 0.5f * s;
        // Les pattes sont de la couleur du corps à 42 % : sombres, pour contraster avec le corps.
        const NkColor leg = color.Shade(0.42f);

        // hOff : la position de la tête devant le centre ; hR : son rayon ; hZ : sa hauteur.
        const float hOff = 6.4f * s;
        const float hR = 7.8f * s;
        const float hZ = zb + s;
        // Les trois segments, de l'abdomen à la tête : position le long du corps, rayon, et hauteur relative.
        const float off[3] = {-7.0f, -0.2f, 6.4f};
        const float rad[3] = {7.4f, 4.3f, 7.8f};
        const float zo[3] = {0.f, 0.5f, 1.f};
        for (int i = 0; i < 3; ++i)
        {
            float wx = x + fx * off[i] * s;
            float wy = y + fy * off[i] * s;
            float wz = zb + zo[i] * s;
            q.Sphere(cam.Depth(wx, wy, wz), wx, wy, wz, rad[i] * s, color);
        }
        // (hx, hy) : le centre de la tête ; headDepth : sa profondeur, que les antennes et les yeux reprennent.
        const float hx = x + fx * hOff;
        const float hy = y + fy * hOff;
        const float headDepth = cam.Depth(0, hy, hZ);

        // Les six pattes : côté gauche (-1) puis droit (+1), arrière (i = 0), milieu (1), avant (2).
        for (int side = -1; side <= 1; side += 2)
        {
            for (int i = 0; i < 3; ++i)
            {
                // -1 arrière, 0 milieu, +1 avant.
                float u = i - 1.f;
                // Phase de la patte : tripode (voir l'en-tête).
                float ph = phase_ + ((i + (side > 0 ? 1 : 0)) & 1) * NK_PI_F;
                // stride : le balancement avant arrière du pied ; lf : la levée du pied (seulement dans la moitié avancée du cycle).
                float stride = NkSin(ph) * 2.4f * s;
                float lf = NkMax(0.f, NkCos(ph)) * 2.6f * s;
                // (x0, y0) : l'attache de la patte sur le corps.
                float x0 = x + fx * (u * 3.2f - 0.6f) * s;
                float y0 = y + fy * (u * 3.2f - 0.6f) * s;
                // reach : la portée latérale de la patte, un peu plus courte aux extrémités.
                float reach = (11.0f - 1.6f * (u < 0 ? -u : u)) * s;
                // (ex, ey) : le pied (extrémité) ; (kx, ky) : le genou (à mi-chemin).
                float ex = x0 + px * side * reach + fx * (u * 4.4f * s + stride);
                float ey = y0 + py * side * reach + fy * (u * 4.4f * s + stride);
                float kx = x0 + px * side * reach * .5f + fx * (u * 1.6f * s + stride * .5f);
                float ky = y0 + py * side * reach * .5f + fy * (u * 1.6f * s + stride * .5f);
                // A : l'attache, K : le genou (levé de 2,2 plus la levée du pied), F : le pied, projetés à l'écran.
                NkVec2f A = cam.Project(x0, y0, zb - 1.0f * s);
                NkVec2f K = cam.Project(kx, ky, zb + 2.2f * s + lf);
                NkVec2f F = cam.Project(ex, ey, lf);
                // Profondeur de la patte : la moyenne des y, légèrement sous le corps pour que le corps passe devant.
                float dp = cam.Depth(0, (y0 + ky + ey) / 3.f, zb - 2.f * s);
                // Cuisse puis tibia, effilés ; puis un petit pied.
                q.Taper(dp, A, K, 2.1f * s, 1.7f * s, leg);
                q.Taper(dp, K, F, 1.7f * s, 0.9f * s, leg);
                q.Ellipse(dp, F, 1.0f * s, 1.0f * s * cam.tilt + .2f * s, leg);
            }
        }

        // Antennes : deux segments (tige puis coude), fines et sombres, et un petit bouton au bout.
        // sway : un balancement lent de la pointe.
        float sway = NkSin(t_ * 5) * 0.8f * s;
        for (int side = -1; side <= 1; side += 2)
        {
            // (bx, by, bz) : la base sur la tête ; (mx, my) : le coude ; (tx, ty) : la pointe, avec le balancement.
            float bx = hx + (fx * 3.0f + px * side * 2.6f) * s;
            float by = hy + (fy * 3.0f + py * side * 2.6f) * s;
            float bz = hZ + 5.4f * s;
            float mx = bx + (fx * 4.0f + px * side * 2.0f) * s;
            float my = by + (fy * 4.0f + py * side * 2.0f) * s;
            float tx = mx + (fx * 2.0f + px * side * 4.0f) * s + px * side * sway;
            float ty = my + (fy * 2.0f + py * side * 4.0f) * s + py * side * sway;
            NkVec2f B = cam.Project(bx, by, bz);
            NkVec2f M = cam.Project(mx, my, bz + 3.0f * s);
            NkVec2f T = cam.Project(tx, ty, bz + 2.6f * s);
            // Les antennes passent devant la tête.
            float dp = headDepth + hR * 1.1f;
            q.Taper(dp, B, M, 1.5f * s, 1.2f * s, leg);
            q.Taper(dp, M, T, 1.2f * s, 0.9f * s, leg);
            q.Ellipse(dp, T, 1.1f * s, 1.1f * s, leg);
        }

        // Yeux : deux billes blanches cerclées de sombre, pupille vers l'avant.
        for (int side = -1; side <= 1; side += 2)
        {
            // e : le centre de l'oeil ; pu : un point plus en avant, vers lequel la pupille est tirée.
            NkVec2f e = cam.Project(hx + (fx * 3.6f + px * side * 3.9f) * s, hy + (fy * 3.6f + py * side * 3.9f) * s, hZ + 4.4f * s);
            NkVec2f pu = cam.Project(hx + (fx * 5.0f + px * side * 3.9f) * s, hy + (fy * 5.0f + py * side * 3.9f) * s, hZ + 4.4f * s);
            // Les yeux passent devant la tête ; les trois couches ont des profondeurs à peine différentes (0,01 puis 0,02)
            // pour garder l'ordre contour, blanc, pupille (le tri est stable, mais on ne s'y fie pas).
            float dp = headDepth + hR * 0.95f;
            q.Ellipse(dp, e, 2.5f * s, 2.7f * s, NkColor(30, 20, 25, 255));
            q.Ellipse(dp + 0.01f, e, 1.9f * s, 2.1f * s, NkColor(255, 255, 255, 255));
            q.Ellipse(dp + 0.02f, NkVec2f(e.x + (pu.x - e.x) * .6f, e.y + (pu.y - e.y) * .6f + .2f * s), 1.0f * s, 1.2f * s, NkColor(25, 15, 20, 255));
        }
    }

    // Emit ANCIEN (04/11/2026), gardé et non appelé : « on ne supprime pas ». Raison du retrait : pattes et antennes
    // épaisses, de la couleur du corps et sans contour, qui fusionnaient en tache quand les fourmis se serrent.
    // Style "chibi" : grosse tête + abdomen, oreilles-antennes épaisses, pattes courtes, gros yeux.
    void EmitAncien(RenderQueue& q, const NkView25D& cam) const
    {
        const float s = scale;
        const float fx = NkCos(heading);
        const float fy = NkSin(heading);
        const float px = -fy;
        const float py = fx;
        const float zb = 6.f * s + NkSin(phase_ * 2) * 0.5f * s;
        const NkColor leg = color.Shade(0.82f);
        const NkColor hi = color.Shade(1.35f);

        // Corps : abdomen, petite taille (cache la jonction), grosse tête, profondeur = centre de la sphère
        const float hOff = 5.5f * s;
        const float hR = 8.5f * s;
        const float hZ = zb + s;
        const float off[3] = {-6.5f, -0.5f, 5.5f};
        const float rad[3] = {7.5f, 4.2f, 8.5f};
        const float zo[3] = {0.f, 0.5f, 1.f};
        for (int i = 0; i < 3; ++i)
        {
            float wx = x + fx * off[i] * s;
            float wy = y + fy * off[i] * s;
            float wz = zb + zo[i] * s;
            q.Sphere(cam.Depth(wx, wy, wz), wx, wy, wz, rad[i] * s, color);
        }
        const float hx = x + fx * hOff;
        const float hy = y + fy * hOff;
        const float headDepth = cam.Depth(0, hy, hZ);

        // Pattes courtes et épaisses, même couleur que le corps, attachées à la taille (3 paires).
        // Marche en tripode : les pattes d'un côté sont en opposition de phase avec celles de l'autre (+ PI).
        const int n = legPairs < 3 ? 3 : (legPairs > 4 ? 4 : legPairs);
        for (int side = -1; side <= 1; side += 2)
        {
            for (int i = 0; i < n; ++i)
            {
                // u : la position sur le corps ; un : la même, ramenée de -1 (arrière) à +1 (avant).
                float u = i - (n - 1) * .5f;
                float un = u / ((n - 1) * .5f);
                float along = (u * 3.4f - 1.f) * s;
                float ph = phase_ + i * 2.09f + (side > 0 ? NK_PI_F : 0);
                float stride = NkSin(ph) * 2.2f * s;
                float lf = NkMax(0.f, NkCos(ph)) * 2.5f * s;
                float x0 = x + fx * along;
                float y0 = y + fy * along;
                float reach = (11.5f - 1.5f * (un < 0 ? -un : un)) * s;
                float ex = x0 + px * side * reach + fx * (un * 4.f * s + stride);
                float ey = y0 + py * side * reach + fy * (un * 4.f * s + stride);
                float kx = x0 + px * side * reach * .55f + fx * (un * 1.5f * s + stride * .5f);
                float ky = y0 + py * side * reach * .55f + fy * (un * 1.5f * s + stride * .5f);
                NkVec2f A = cam.Project(x0, y0, zb - 1.5f * s);
                NkVec2f K = cam.Project(kx, ky, zb + 1.5f * s + lf);
                NkVec2f F = cam.Project(ex, ey, lf);
                // Côté lointain derrière, proche devant.
                float dp = cam.Depth(0, (y0 + ky + ey) / 3.f, zb - 2.f * s);
                q.Taper(dp, A, K, 3.4f * s, 2.8f * s, leg);
                q.Taper(dp, K, F, 2.8f * s, 1.2f * s, leg);
                q.Ellipse(dp, F, 1.1f * s, 1.1f * s * cam.tilt + .2f * s, color.Shade(0.7f));
            }
        }

        // Antennes = grosses "oreilles" effilées avec reflet, posées sur le dessus de la tête (toujours devant la tête)
        float sway = NkSin(t_ * 5) * 0.8f * s;
        for (int side = -1; side <= 1; side += 2)
        {
            float bx = hx + px * side * 4.2f * s;
            float by = hy + py * side * 4.2f * s;
            float bz = hZ + 6.0f * s;
            float tx = bx + (fx * 2.0f + px * side * 5.0f) * s + px * side * sway;
            float ty = by + (fy * 2.0f + py * side * 5.0f) * s + py * side * sway;
            NkVec2f B = cam.Project(bx, by, bz);
            NkVec2f T = cam.Project(tx, ty, bz + 5.5f * s);
            float dp = headDepth + hR * 1.1f;
            q.Taper(dp, B, T, 3.6f * s, 2.0f * s, leg);
            q.Ellipse(dp, T, 1.0f * s, 1.0f * s, leg);
            // Reflet intérieur.
            q.Taper(dp + 0.01f, B, T, 1.5f * s, 0.8f * s, hi);
        }

        // Yeux : grands, blancs, pupille vers l'avant, paupière sombre inclinée (regard déterminé), toujours visibles
        NkVec2f hc = cam.Project(hx, hy, hZ);
        for (int side = -1; side <= 1; side += 2)
        {
            NkVec2f e = cam.Project(hx + (fx * 4.4f + px * side * 4.6f) * s, hy + (fy * 4.4f + py * side * 4.6f) * s, hZ + 5.2f * s);
            NkVec2f pu = cam.Project(hx + (fx * 5.6f + px * side * 4.6f) * s, hy + (fy * 5.6f + py * side * 4.6f) * s, hZ + 5.2f * s);
            // Vers le centre de la tête (écran).
            float dir = hc.x > e.x ? 1.f : -1.f;
            float dp = headDepth + hR * 0.95f;
            q.Ellipse(dp, e, 2.7f * s, 2.9f * s, NkColor(255, 255, 255, 255));
            q.Ellipse(dp + 0.01f, NkVec2f(e.x + (pu.x - e.x) * .7f, e.y + (pu.y - e.y) * .7f + .3f * s), 1.4f * s, 1.6f * s, NkColor(25, 15, 20, 255));
            q.Line(dp + 0.02f, NkVec2f(e.x + dir * 3.f * s, e.y - 1.9f * s), NkVec2f(e.x - dir * 3.f * s, e.y - 3.0f * s), 1.3f * s, NkColor(55, 15, 30, 255));
        }
    }

private:
    // Générateur pseudo-aléatoire propre à la fourmi (choix du côté de dégagement).
    NkLcg32 rng_;
    // Âge (secondes) : apparition en douceur.
    float born_ = 0;
    // (tx_, ty_) : la cible finale ; phase_ : la phase de marche des pattes ; t_ : le temps de vie (balancement des antennes) ;
    // (dropX_, dropY_) : où déposer le bloc (ordre Fetch) ; (avX_, avY_) et avT_ : l'évitement demandé par Nudge et son temps restant.
    float tx_ = 0;
    float ty_ = 0;
    float phase_ = 0;
    float t_ = 0;
    float dropX_ = 0;
    float dropY_ = 0;
    float avX_ = 0;
    float avY_ = 0;
    float avT_ = 0;
    // Les points de passage du chemin (au plus 160), leur nombre (nwp_) et l'indice du prochain (iwp_).
    NkVec2f wp_[160];
    int nwp_ = 0;
    int iwp_ = 0;
    // Détection d'impasse : stuckAcc_ le temps écoulé depuis la dernière mesure ; (sx_, sy_) la position de la dernière mesure ;
    // esc_ le temps restant de dégagement et escSign_ son côté (+1 ou -1) ; stuck_ le nombre de blocages constatés.
    float stuckAcc_ = 0;
    float sx_ = 0;
    float sy_ = 0;
    float esc_ = 0;
    float escSign_ = 1;
    int stuck_ = 0;
    // Vrai tant que la fourmi a une cible.
    bool hasTarget_ = false;
    // 0 libre, 1 va chercher, 2 porte.
    int state_ = 0;
    // Le bloc concerné (cherché ou porté).
    Box* box_ = nullptr;
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
    // La vue 2.5D de la scène : projection, profondeur, ombre. Réglée par Gameplay::SetWindow selon la forme de l'écran.
    NkView25D camera;

    // Crée une entité de type T, la range dans la scène et rend un pointeur dessus (la scène en reste propriétaire).
    template <class T>
    T* Add()
    {
        NkUniquePtr<T> p = NkMakeUnique<T>();
        T* raw = p.Get();
        // Conversion explicite du pointeur unique de T vers Entity (déplacement, pas de copie).
        items_.PushBack(NkUniquePtr<Entity>(static_cast<NkUniquePtr<T>&&>(p)));
        return raw;
    }

    // Vide la scène (changement de niveau) : les entités sont détruites par leurs pointeurs uniques.
    void Clear()
    {
        items_.Clear();
        cmds_.Clear();
    }

    // Fait avancer toute la scène de dt secondes : mise à jour, collisions, seconde mise à jour, retrait des morts.
    void Update(float dt)
    {
        for (usize i = 0; i < items_.Size(); ++i)
        {
            items_[i]->Update(dt);
        }
        // Les collisions se règlent APRÈS le mouvement, puis LateUpdate replace ce qui dépend des positions finales.
        Resolve();
        for (usize i = 0; i < items_.Size(); ++i)
        {
            items_[i]->LateUpdate();
        }
        RemoveDead();
    }

    // Dessine la scène : d'abord toutes les ombres, puis toutes les commandes triées par profondeur.
    //   dl : la liste de dessin.
    void Render(Dl& dl)
    {
        // 1. Les ombres, au sol : elles passent derrière tout.
        for (usize i = 0; i < items_.Size(); ++i)
        {
            items_[i]->DrawShadow(dl, camera);
        }
        // 2. Chaque entité émet ses commandes dans la file.
        cmds_.Clear();
        RenderQueue q{cmds_};
        for (usize i = 0; i < items_.Size(); ++i)
        {
            items_[i]->Emit(q, camera);
        }
        // Tri fusion STABLE par profondeur croissante : du plus loin au plus près.
        NkStableSort(
            cmds_.Data(),
            cmds_.Data() + cmds_.Size(),
            [](const DrawCmd& a, const DrawCmd& b)
            {
                return a.depth < b.depth;
            });
        // Project rend des PIXELS : les positions suivent la vue toutes seules, mais les TAILLES (épaisseurs, rayons) sont
        // en unités de plan ; on les multiplie par l'échelle de la vue pour que tout grandisse ensemble.
        const float k = camera.Scale();
        // 3. On dessine les commandes dans l'ordre trié.
        for (usize i = 0; i < cmds_.Size(); ++i)
        {
            const DrawCmd& c = cmds_[i];
            switch (c.kind)
            {
                case CMD_LINE:
                    dl.AddLine(c.p0, c.p1, c.col, c.a * k);
                    break;
                case CMD_TAPER:
                    dl.AddTaperedLine(c.p0, c.p1, c.col, c.a * k, c.b * k);
                    break;
                case CMD_ELLIPSE:
                    dl.AddEllipseFilled(c.p0, c.a * k, c.b * k, c.col);
                    break;
                case CMD_SPHERE:
                    DrawSphere(dl, camera, c.p0.x, c.p0.y, c.a, c.b * k, c.col);
                    break;
                case CMD_CUSTOM:
                    static_cast<const ICustomDraw*>(c.ptr)->DrawPiece(dl, camera, (int)c.a);
                    break;
                case CMD_BOX:
                    static_cast<const Box*>(c.ptr)->DrawBody(dl, camera);
                    break;
            }
        }
    }

private:
    // Retire les entités marquées `dead` : on range les vivantes dans un nouveau vecteur, puis il remplace l'ancien.
    void RemoveDead()
    {
        usize dead = 0;
        for (usize i = 0; i < items_.Size(); ++i)
        {
            if (items_[i]->dead)
            {
                ++dead;
            }
        }
        // Aucune morte : rien à faire (le cas le plus courant, on évite de recopier le vecteur).
        if (!dead)
        {
            return;
        }
        NkVector<NkUniquePtr<Entity>> live;
        for (usize i = 0; i < items_.Size(); ++i)
        {
            if (!items_[i]->dead)
            {
                live.PushBack(static_cast<NkUniquePtr<Entity>&&>(items_[i]));
            }
        }
        // Les morts sont détruits avec items_ : l'ancien vecteur est remplacé, ses entités mortes sont libérées.
        items_ = static_cast<NkVector<NkUniquePtr<Entity>>&&>(live);
    }

    // Les collisions de chaque fourmi : contre le plateau, contre les autres fourmis, contre les boîtes posées.
    // Chaque contact repousse la fourmi hors de l'obstacle ET appelle Nudge pour qu'elle contourne.
    void Resolve()
    {
        usize n = items_.Size();
        for (usize i = 0; i < n; ++i)
        {
            Entity* a = items_[i].Get();
            // Seules les fourmis se déplacent et ont des collisions à résoudre.
            if (a->kind != KIND_ANT)
            {
                continue;
            }
            Ant* A = static_cast<Ant*>(a);
            // Obstacles génériques (le plateau) : toute entité de sorte KIND_BOARD.
            for (usize j = 0; j < n; ++j)
            {
                Entity* o = items_[j].Get();
                if (o->kind == KIND_BOARD)
                {
                    float nx;
                    float ny;
                    if (o->ResolveCircle(A->x, A->y, A->Radius(), nx, ny))
                    {
                        A->Nudge(nx, ny);
                    }
                }
            }
            // Fourmis et boîtes : les sortes au-delà de KIND_BOX (plateau, nid) sont ignorées.
            for (usize j = 0; j < n; ++j)
            {
                Entity* b = items_[j].Get();
                if (b == a || b->kind > KIND_BOX)
                {
                    continue;
                }
                if (b->kind == KIND_ANT)
                {
                    // Fourmi / fourmi (traité une fois) : on ne regarde que les paires (i, j) avec j >= i.
                    if (j < i)
                    {
                        continue;
                    }
                    Ant* B = static_cast<Ant*>(b);
                    // Une fourmi « fantôme » traverse les autres (voir Ant::ghost).
                    if (A->ghost > 0 || B->ghost > 0)
                    {
                        continue;
                    }
                    // d : la distance entre les centres ; md : la distance minimale (la somme des rayons).
                    float dx = B->x - A->x;
                    float dy = B->y - A->y;
                    float d = NkVec2f(dx, dy).Len();
                    float md = A->Radius() + B->Radius();
                    if (d < md && d > 1e-4f)
                    {
                        // (nx, ny) : la normale de A vers B ; pu : la moitié du chevauchement, que chacune absorbe.
                        float nx = dx / d;
                        float ny = dy / d;
                        float pu = (md - d) * .5f;
                        // Chacune recule de la moitié du chevauchement, en sens opposés...
                        A->x -= nx * pu;
                        A->y -= ny * pu;
                        B->x += nx * pu;
                        B->y += ny * pu;
                        // ... et se détourne de l'autre.
                        A->Nudge(-nx, -ny);
                        B->Nudge(nx, ny);
                    }
                }
                else
                {
                    // Fourmi / boîte (non portée) : une boîte portée suit sa porteuse, elle n'est pas un obstacle.
                    Box* B = static_cast<Box*>(b);
                    if (B->carried)
                    {
                        continue;
                    }
                    float cx;
                    float cy;
                    B->Closest(A->x, A->y, cx, cy);
                    float dx = A->x - cx;
                    float dy = A->y - cy;
                    float d = NkVec2f(dx, dy).Len();
                    float r = A->Radius();
                    if (d < r)
                    {
                        float nx;
                        float ny;
                        if (d > 1e-4f)
                        {
                            nx = dx / d;
                            ny = dy / d;
                        }
                        else
                        {
                            // Le centre de la fourmi est dans la boîte : on la pousse depuis le centre de la boîte.
                            float ex = A->x - B->x;
                            float ey = A->y - B->y;
                            float el = NkMax(1e-4f, NkVec2f(ex, ey).Len());
                            nx = ex / el;
                            ny = ey / el;
                        }
                        // Repoussée de la profondeur de pénétration (r - d), puis détournée.
                        A->x += nx * (r - d);
                        A->y += ny * (r - d);
                        A->Nudge(nx, ny);
                    }
                }
            }
        }
    }

    // Les entités (la scène en est propriétaire) et la file de commandes de la dernière image.
    NkVector<NkUniquePtr<Entity>> items_;
    NkVector<DrawCmd> cmds_;
};

} // namespace nk25d
