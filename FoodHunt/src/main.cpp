// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// main.cpp : FoodHunt : plateau de cubes 2.5D, stock en rayon de miel, paniers, fourmis par couleur.
// Migré le 04/11/2026 : l'application hérite de NkCanvasGuiApp (NKCanvas) et dessine sur un NkGuiDrawList ; la
// police est celle, embarquée, de NKFont. Réécrit le 05/11/2026 : les vingt-cinq niveaux sont des images embarquées
// (LevelLibrary), l'écran peut avoir n'importe quelle forme portrait, le bot est une option (--bot) et le banc
// `--bot --verifier-niveaux` joue les vingt-cinq niveaux sans fenêtre.
//
// RÔLE DE CE FICHIER : c'est le point d'entrée. Il ne contient AUCUNE règle de jeu. Il fait le lien entre le moteur
// (NKCanvas : fenêtre, boucle, entrées souris et doigt, liste de dessin) et le jeu, qui vit dans les autres fichiers :
//   - Nk25DLevels.h  : la bibliothèque des 25 niveaux (grilles embarquées dans l'exécutable) ;
//   - Nk25DBoard.h   : le plateau (les cubes, le stock, les paniers) ;
//   - Nk25DGame.h    : la partie (Gameplay) : fourmis, bot, mise en page, interface, statistiques ;
//   - Nk25DArt.h     : le dessin (herbe, nappe, cubes, fourmis) ;
//   - NkView25D.h    : la vue 2.5D (projection, profondeur, ombre).
// Ici, on lit la ligne de commande, on charge un niveau, on transmet à Gameplay les clics et le temps, et on
// dessine dans l'ordre fond, scène, interface.
//
// Options de la ligne de commande (toutes facultatives) :
//   --bot                    le jeu joue seul (essais) ; sans cette option on joue au doigt ou à la souris
//   --verifier-niveaux       (avec --bot) banc : joue les 25 niveaux, accéléré, SANS fenêtre ni GPU ; le code de sortie
//                            est 0 si tout passe, sinon le nombre de niveaux en échec ; le rapport va dans un fichier
//   --rapport=<fichier>      où écrire le rapport du banc (défaut : foodhunt_banc.txt)
//   --casser-niveau=<n>      contre-épreuve : rend le niveau n injouable (aucun tiroir) ; le banc doit rougir
//   --ancien-comportement    contre-épreuve : rétablit l'ancienne conduite des fourmis (naissance au nid, plusieurs blocs)
//   --fenetre=<L>x<H>        taille de la fenêtre (essais d'écrans : 480x960, 1080x2400, 768x1024...)
//   --niveau=<n>             démarre au niveau n (1..25) ; --avance=<s> simule s secondes de bot avant d'afficher
//   --capture=<png> --capture-frame=<n>   (moteur) écrit une image de la trame n et sort
#include "NKWindow/NKMain.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "NKFileSystem/NkFile.h"
#include "Nk25DGame.h"
#include "Nk25DLevels.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// -----------------------------------------------------------------------------
// FoodHunt : l'application. Elle hérite de NkCanvasGuiApp, qui fournit la fenêtre, la boucle principale et les
// rappels (OnTick, OnDraw, OnLayout, OnPointer...) que nous redéfinissons plus bas.
// -----------------------------------------------------------------------------
class FoodHunt : public NkCanvasGuiApp {
public:
    // Taille LOGIQUE de référence du monde, en unités de plan (pas en pixels d'écran). Tout le jeu est pensé dans ce
    // rectangle de 540 x 1000 ; la mise en page (Gameplay::SetWindow) l'adapte ensuite à la vraie fenêtre.
    static constexpr float kRefW = 540.f;
    static constexpr float kRefH = 1000.f;

    // Le bot n'est plus le comportement par défaut : il faut l'option --bot pour qu'il joue.
    static constexpr bool kAutoPlay = false;

    // Constructeur : règle la fenêtre de départ (480 x 960 pixels, portrait) et la couleur d'effacement, un vert
    // herbe (R 150, V 200, B 100, opaque) qui se voit si une bande de l'écran n'est pas couverte par le décor.
    FoodHunt()
    {
        Config().title = "FoodHunt";
        Config().width = 480;
        Config().height = 960;
        Config().clearColor = NkColor2D(150, 200, 100, 255);
    }

protected:
    // ---- Ligne de commande : le banc répond AVANT toute fenêtre ---------------------------------

    // Vrai si la chaîne s commence par le préfixe p (petit raccourci pour lire les options).
    static bool StartsWith(const NkString& s, const char* p)
    {
        return s.StartsWith(p);
    }

    // Lit un entier positif écrit en chiffres au début de p, et s'arrête au premier caractère qui n'est pas un chiffre.
    //   p   : le texte à lire (peut être nul ou vide).
    //   def : la valeur rendue si p est nul ou vide.
    // Rend l'entier lu. Pas de signe moins : les options n'en ont pas besoin.
    static int ParseInt(const char* p, int def)
    {
        // Rien à lire : on rend la valeur par défaut.
        if (!p || !*p)
        {
            return def;
        }
        int v = 0;
        // On accumule chiffre par chiffre : v = v * 10 + chiffre.
        for (; *p >= '0' && *p <= '9'; ++p)
        {
            v = v * 10 + (*p - '0');
        }
        return v;
    }

    // Lit la ligne de commande. Appelé par le moteur AVANT la création de la fenêtre.
    //   args : les arguments, un par élément.
    // Rend un entier (le code de sortie du programme) si le banc a tout fait sans fenêtre, sinon rien : le moteur
    // continue alors normalement et ouvre la fenêtre.
    NkOptional<int> OnCommandLine(const NkVector<NkString>& args) override
    {
        for (usize i = 0; i < args.Size(); ++i)
        {
            const NkString& a = args[i];
            if (a == "--bot")
            {
                mBot = true;
            }
            else if (a == "--verifier-niveaux")
            {
                mBanc = true;
            }
            else if (a == "--ancien-comportement")
            {
                mLegacy = true;
            }
            else if (StartsWith(a, "--clic-a="))
            {
                // Essai : un clic sur la tête de la colonne 1 à la trame n (9 = longueur de "--clic-a=").
                mClicA = ParseInt(a.CStr() + 9, -1);
            }
            else if (StartsWith(a, "--clic-b="))
            {
                // Essai : un second clic (colonne 2) à la trame n.
                mClicB = ParseInt(a.CStr() + 9, -1);
            }
            else if (StartsWith(a, "--marges="))
            {
                // Essai : une fausse encoche, quatre nombres haut,bas,gauche,droite en pixels (ex. --marges=40,20,0,0).
                const char* q = a.CStr() + 9;
                float m[4] = {0, 0, 0, 0};
                for (int j = 0; j < 4 && *q; ++j)
                {
                    m[j] = (float)ParseInt(q, 0);
                    // On saute les chiffres déjà lus, puis la virgule qui sépare les nombres.
                    while (*q >= '0' && *q <= '9')
                    {
                        ++q;
                    }
                    if (*q == ',')
                    {
                        ++q;
                    }
                }
                mSafe = NkSafeAreaInsets(m[0], m[1], m[2], m[3]);
                // Les marges viennent de la ligne de commande : OnLayout ne doit pas les écraser par celles du système.
                mMargesForcees = true;
            }
            else if (a == "--decaler-clic")
            {
                // Contre-épreuve : les clics du bot sont décalés de 300 pixels et ratent leur cible ; le banc doit rougir.
                mDecale = 300.f;
            }
            else if (StartsWith(a, "--casser-niveau="))
            {
                mCasser = ParseInt(a.CStr() + 16, 0);
            }
            else if (StartsWith(a, "--rapport="))
            {
                mRapport = NkString(a.CStr() + 10);
            }
            else if (StartsWith(a, "--niveau="))
            {
                // L'utilisateur compte de 1 à 25, le code de 0 à 24 : on borne puis on retire 1.
                // mSolo : on ne joue que ce niveau-là (le banc ne fait pas les 25).
                mStart = math::NkClamp(ParseInt(a.CStr() + 9, 1), 1, 25) - 1;
                mSolo = true;
            }
            else if (StartsWith(a, "--avance="))
            {
                mAvance = (float)ParseInt(a.CStr() + 9, 0);
            }
            else if (StartsWith(a, "--fenetre="))
            {
                // Forme attendue : LxH, par exemple 1080x2400. Défauts : 480 et 960.
                const char* q = a.CStr() + 10;
                int w = ParseInt(q, 480);
                while (*q >= '0' && *q <= '9')
                {
                    ++q;
                }
                if (*q == 'x')
                {
                    ++q;
                }
                int h = ParseInt(q, 960);
                Config().width = (uint32)w;
                Config().height = (uint32)h;
                // Le banc (sans fenêtre) a besoin de la même taille pour calculer la mise en page.
                mFenW = (float)w;
                mFenH = (float)h;
            }
        }
        // Le banc tourne ici, sans fenêtre ni GPU, et son résultat devient le code de sortie du programme.
        if (mBanc && mBot)
        {
            return NkOptional<int>(RunBench());
        }
        return NkOptional<int>();
    }

    // La police suit l'échelle de l'écran : le texte est dessiné à ~24 px logiques puis mis à l'échelle par le même facteur.
    //   lay : la mise en page de l'écran (largeur et hauteur en pixels).
    // Rend la taille du corps de texte en pixels, bornée entre 12 (lisible) et 72 (pas démesurée).
    float32 TaillePoliceCorps(const NkLayoutInfo& lay) const noexcept override
    {
        // Même formule d'échelle que la vue : le texte reste proportionné au plateau.
        float s = nk25d::NkView25D::ScaleFor((float)lay.width, (float)lay.height, kRefW, kRefH);
        float px = 24.f * s;
        return px < 12.f ? 12.f : (px > 72.f ? 72.f : px);
    }

    // Appelé par NkCanvasGuiApp::OnInit une fois le contexte graphique et les polices prêts.
    // Rend true si tout est prêt, false pour arrêter le démarrage (aucun niveau, ou niveau impossible à charger).
    bool OnGuiInit() override
    {
        // La police embarquée de NKFont sert à tout le texte du jeu.
        nk25d::GameFont() = FontBody();
        if (mLib.LoadEmbedded() <= 0)
        {
            return false;
        }
        // Niveau de départ : celui demandé par --niveau, ramené dans les bornes de la bibliothèque.
        mLevel = math::NkClamp(mStart, 0, mLib.Count() - 1);
        if (!StartLevel(mLevel))
        {
            return false;
        }
        // Essais : avancer la partie (le bot CLIQUE) avant la capture, pour photographier un jeu en cours.
        if (mAvance > 0.f)
        {
            // Le bot réfléchit toutes les 0.3 s de jeu pendant cet avancement.
            mGame.SetBotPeriod(0.3f);
            // Pas fixe de 1/30 s ; on s'arrête au temps demandé ou dès que la partie est finie.
            for (float t = 0.f; t < mAvance && mGame.GetState() == nk25d::Gameplay::PLAYING; t += 1.f / 30.f)
            {
                mGame.Tick(1.f / 30.f);
                BotStep(1.f / 30.f);
            }
        }
        return true;
    }

    // Appelé à chaque image.
    //   dt : le temps écoulé depuis l'image précédente, en secondes.
    void OnTick(float32 dt) override
    {
        // Compteur de trames : il sert seulement aux clics d'essai (--clic-a et --clic-b).
        ++mTick;
        {
            float lx;
            float ly;
            // Colonne à cliquer à cette trame : 1 pour --clic-a, 2 pour --clic-b, -1 si aucun clic n'est prévu.
            int col = mTick == mClicA ? 1 : (mTick == mClicB ? 2 : -1);
            if (col >= 0 && mGame.CellPoint(col, 0, lx, ly))
            {
                // Le clic d'essai passe par le même chemin qu'un vrai doigt : coordonnées logiques -> pixels -> Tap.
                Tap(lx * mGame.ViewScale() + mGame.SafeLeft(), ly * mGame.ViewScale() + mGame.SafeTop());
            }
        }
        // La partie avance du temps réel écoulé.
        mGame.Tick(dt);
        // Le bot, lui, avance du temps multiplié par la vitesse de jeu (TimeScale), pour rester synchrone avec la partie.
        if (mBot)
        {
            BotStep(dt * mGame.TimeScale());
        }
        // Le bot enchaîne les niveaux : après 2.5 s d'écran de fin, il passe au suivant si gagné (en reboucle après le
        // dernier), ou rejoue le même niveau si perdu.
        if (mBot && mGame.GetState() != nk25d::Gameplay::PLAYING && mGame.EndTime() > 2.5f)
        {
            StartLevel(mGame.GetState() == nk25d::Gameplay::WON ? (mLevel + 1) % mLib.Count() : mLevel);
        }
    }

    // Tout le dessin se fait en coordonnées LOGIQUES, puis les sommets sont mis à l'échelle de l'écran d'un seul coup :
    // le jeu n'a ainsi qu'UNE mise en page à connaître (voir Gameplay::SetWindow), quelle que soit la forme de l'écran.
    //   dl : la liste de dessin du moteur, qui reçoit les sommets.
    void OnDraw(nkgui::NkGuiDrawList& dl) override
    {
        // Deux espaces de coordonnées : la scène (cubes, fourmis) passe par NkView25D::Project et sort DÉJÀ en pixels ;
        // le fond et l'interface sont dessinés en coordonnées logiques du plan de la vue puis mis à l'échelle (k).
        const float k = mGame.ViewScale();
        // On retient où commencent nos sommets, pour ne mettre à l'échelle que ceux qu'on ajoute (pas les précédents).
        usize v0 = dl.vtx.Size();
        // 1. Le fond : herbe, assiette, nappe. Dessiné en premier, donc tout le reste passe devant.
        mGame.DrawBackground(dl);
        // Fond en coordonnées logiques : échelle k puis décalage de la zone sûre.
        Scale(dl, v0, k, mGame.SafeLeft(), mGame.SafeTop());
        // 2. La scène : cubes et fourmis, triés par profondeur (algorithme du peintre) ; ils sortent déjà en pixels.
        mScene.Render(dl);
        // 3. Les effets et l'interface repartent d'un nouveau point de départ, car la scène ne doit pas être remise à l'échelle.
        v0 = dl.vtx.Size();
        // Miettes et confettis.
        mGame.DrawEffects(dl);
        // Paniers, rayon de miel, boutons, écrans de fin : par-dessus tout le reste.
        mGame.DrawUI(dl);
        // Effets et interface sont en coordonnées logiques : même mise à l'échelle que le fond.
        Scale(dl, v0, k, mGame.SafeLeft(), mGame.SafeTop());
    }

    // Redimensionnement, rotation : la forme de l'écran change, le monde du jeu ne bouge pas.
    //   lay : la nouvelle mise en page (taille en pixels, zone sûre).
    void OnLayout(const NkLayoutInfo& lay) override
    {
        NkCanvasGuiApp::OnLayout(lay);
        mWinW = (float)lay.width;
        mWinH = (float)lay.height;
        // Encoche, barre de statut, indicateur de geste : le système donne la zone sûre, sauf si --marges l'a imposée.
        if (!mMargesForcees)
        {
            mSafe = lay.safeArea;
        }
        // Ordre voulu : d'abord la zone sûre, ensuite la fenêtre, car SetWindow s'appuie sur les marges pour sa mise en page.
        mGame.SetSafeArea(mSafe.top, mSafe.bottom, mSafe.left, mSafe.right);
        mGame.SetWindow(mWinW, mWinH);
    }

    // Entrée souris / doigt : un appui est un tap, en pixels client -> coordonnées logiques (division par l'échelle).
    //   ptr : l'événement (phase, position x et y en pixels).
    // Rend true si l'événement est consommé (un appui), false sinon (déplacement, relâchement).
    bool OnPointer(const NkPointer& ptr) override
    {
        if (ptr.phase != NkPointerPhase::NK_POINTER_DOWN)
        {
            return false;
        }
        Tap(ptr.x, ptr.y);
        return true;
    }

private:
    // LE chemin du clic : pixels d'écran -> coordonnées logiques (division par l'échelle de la vue) -> Gameplay::OnTap.
    // La souris, le doigt ET le bot passent tous par ici : le bot ne pose plus un bloc par un appel direct.
    //   px, py : la position du clic en pixels d'écran.
    void Tap(float px, float py)
    {
        const float k = mGame.ViewScale();
        // On retire le décalage de la zone sûre, puis on divise par l'échelle pour retrouver les coordonnées logiques.
        nk25d::Gameplay::Action a = mGame.OnTap((px - mGame.SafeLeft()) / k, (py - mGame.SafeTop()) / k);
        // Le jeu répond par une action à exécuter côté application : passer au niveau suivant, ou recommencer.
        if (a == nk25d::Gameplay::ACT_NEXT)
        {
            StartLevel((mLevel + 1) % mLib.Count());
        }
        else if (a == nk25d::Gameplay::ACT_RESTART)
        {
            StartLevel(mLevel);
        }
    }

    // Le bot « appuie » : il rend un point de l'écran logique, on le convertit en pixels (comme un vrai doigt) et on tape.
    //   dt : le temps de jeu à faire avancer au bot, en secondes.
    void BotStep(float dt)
    {
        float lx;
        float ly;
        if (mGame.GetState() == nk25d::Gameplay::PLAYING && mGame.BotTick(dt, lx, ly))
        {
            // mDecale vaut 0 en temps normal ; avec --decaler-clic il fait rater le clic (contre-épreuve du banc).
            Tap(lx * mGame.ViewScale() + mGame.SafeLeft() + mDecale, ly * mGame.ViewScale() + mGame.SafeTop());
        }
    }

    // Met à l'échelle les sommets ajoutés depuis v0 (le plan logique -> les pixels), puis les décale de la zone sûre.
    //   dl     : la liste de dessin.
    //   v0     : indice du premier sommet à transformer.
    //   k      : l'échelle (pixels par unité logique).
    //   dx, dy : le décalage en pixels à ajouter après l'échelle (marges gauche et haut de la zone sûre).
    static void Scale(nkgui::NkGuiDrawList& dl, usize v0, float k, float dx, float dy)
    {
        for (usize i = v0; i < dl.vtx.Size(); ++i)
        {
            dl.vtx[i].pos.x = dl.vtx[i].pos.x * k + dx;
            dl.vtx[i].pos.y = dl.vtx[i].pos.y * k + dy;
        }
    }

    // Charge le niveau i : la scène est vidée, un plateau neuf est posé, le jeu repart de zéro.
    //   i : indice du niveau (0 à 24).
    // Rend false si le niveau n'a pas pu être chargé.
    bool StartLevel(int i)
    {
        mLevel = i;
        // On vide la scène (anciens cubes et fourmis) et on y ajoute un plateau neuf.
        mScene.Clear();
        mBoard = mScene.Add<nk25d::Board>();
        if (!mLib.LoadLevel(i, *mBoard))
        {
            return false;
        }
        // Même ordre que dans OnLayout : zone sûre d'abord, fenêtre ensuite.
        mGame.SetSafeArea(mSafe.top, mSafe.bottom, mSafe.left, mSafe.right);
        mGame.SetWindow(mWinW, mWinH);
        // Le numéro affiché est i + 1 (les joueurs comptent à partir de 1).
        mGame.Init(&mScene, mBoard, i + 1);
        // Contre-épreuve : --casser-niveau rend le niveau choisi injouable.
        if (mCasser == i + 1)
        {
            mGame.BreakLevel();
        }
        // Contre-épreuve : --ancien-comportement rétablit l'ancienne conduite des fourmis.
        if (mLegacy)
        {
            mGame.SetLegacy(true);
        }
        return true;
    }

    // ---- Le BANC : joue les 25 niveaux en pur calcul (aucune fenêtre, aucun GPU) -------------------------
    // Pour chaque niveau : pas fixe de 1/30 s, au plus 15 minutes de jeu, le bot joue. Un niveau PASSE si :
    //   victoire ; somme des numéros du stock = nombre de cubes, couleur par couleur ; sorties = somme des numéros ;
    //   retours au nid = sorties ; aucune sortie sans bloc réservé ; aucune fourmi ressortie ; aucun bloc réservé deux fois ;
    //   aucun bloc réservé disparu. Le code de sortie est le nombre de niveaux en échec.
    // Rend le nombre de niveaux en échec (0 = tout passe), qui devient le code de sortie du programme.
    int RunBench()
    {
        nk25d::LevelLibrary lib;
        int n = lib.LoadEmbedded();
        // Première ligne du rapport : le nombre de niveaux, et une étiquette si une contre-épreuve est active.
        NkString rap = NkFormat("FoodHunt, banc des niveaux ({0} niveaux){1}{2}\n", n, mLegacy ? " [ancien comportement]" : "", mCasser > 0 ? " [niveau cassé]" : "");
        int fails = 0;
        // Épreuve de la VUE : avec une vue NEUTRE (échelle 1), NkView25D::Project doit rendre exactement les mêmes points que
        // l'ancienne formule de Camera ; Unproject doit défaire Project ; et une vue NON neutre doit, elle, différer (contre-épreuve :
        // sans elle, un Project qui ignorerait la vue passerait aussi l'épreuve).
        {
            nk25d::NkView25D v;
            nk25d::CameraAncienne old;
            old.origin = nkentseu::math::NkVec2f(13.f, 27.f);
            // Même inclinaison pour les deux, sinon la comparaison n'aurait aucun sens.
            v.tilt = 0.85f;
            old.tilt = v.tilt;
            v.Neutral(540.f, 1000.f);
            // Le centre est décalé de l'origine de l'ancienne caméra : Project doit alors donner les mêmes pixels.
            v.center = v.size * .5f - old.origin;
            // Une copie dont la fenêtre est deux fois plus grande : son échelle vaut 2, donc ses points doivent différer.
            nk25d::NkView25D w = v;
            w.window = nkentseu::math::NkVec2f(1080.f, 2000.f);
            // Compteurs : écarts de projection, écarts d'aller-retour, points où la vue non neutre diffère, total.
            int bad = 0;
            int badRound = 0;
            int wDiffers = 0;
            int total = 0;
            for (float x = -50.f; x <= 600.f; x += 37.f)
            {
                for (float y = -50.f; y <= 700.f; y += 41.f)
                {
                    for (float z = 0.f; z <= 30.f; z += 7.5f)
                    {
                        // a2 : nouvelle vue ; b2 : ancienne formule ; c2 : vue non neutre ; s1 et s2 : ombres nouvelle et ancienne.
                        nkentseu::math::NkVec2f a2 = v.Project(x, y, z);
                        nkentseu::math::NkVec2f b2 = old.Project(x, y, z);
                        nkentseu::math::NkVec2f c2 = w.Project(x, y, z);
                        nkentseu::math::NkVec2f s1 = v.ShadowPos(x, y, z);
                        nkentseu::math::NkVec2f s2 = old.ShadowPos(x, y, z);
                        ++total;
                        // Tolérance de 0.001 pixel sur la projection, l'ombre et la profondeur.
                        if (nk25d::NkAbs(a2.x - b2.x) > 1e-3f
                            || nk25d::NkAbs(a2.y - b2.y) > 1e-3f
                            || nk25d::NkAbs(s1.x - s2.x) > 1e-3f
                            || nk25d::NkAbs(s1.y - s2.y) > 1e-3f
                            || nk25d::NkAbs(v.Depth(x, y, z) - old.Depth(x, y, z)) > 1e-3f)
                        {
                            ++bad;
                        }
                        // Aller-retour : projeter un point du sol puis le déprojeter doit redonner le point (tolérance 0.01).
                        nkentseu::math::NkVec2f g = v.Unproject(v.Project(x, y, 0.f).x, v.Project(x, y, 0.f).y);
                        if (nk25d::NkAbs(g.x - x) > 1e-2f || nk25d::NkAbs(g.y - y) > 1e-2f)
                        {
                            ++badRound;
                        }
                        // La vue non neutre doit s'écarter de plus d'un pixel de l'ancienne formule.
                        if (nk25d::NkAbs(c2.x - b2.x) > 1.f || nk25d::NkAbs(c2.y - b2.y) > 1.f)
                        {
                            ++wDiffers;
                        }
                    }
                }
            }
            // Réussi si aucun écart, aucun aller-retour faux, et si la vue non neutre diffère sur plus de la moitié des points.
            const bool okVue = bad == 0 && badRound == 0 && wDiffers > total / 2;
            if (!okVue)
            {
                ++fails;
            }
            rap += NkFormat("Vue neutre : {0}  points {1}  ecarts {2}  aller-retour {3}  vue-non-neutre-differe {4}\n", okVue ? "OK" : "ECHEC", total, bad, badRound, wDiffers);
        }
        for (int i = 0; i < n; ++i)
        {
            // Avec --niveau, on ne joue que le niveau demandé.
            if (mSolo && i != mStart)
            {
                continue;
            }
            // Chaque niveau a sa propre scène et son propre plateau : aucun état ne passe d'un niveau au suivant.
            nk25d::Scene scene;
            nk25d::Board* board = scene.Add<nk25d::Board>();
            if (!lib.LoadLevel(i, *board))
            {
                rap += NkFormat("{0} : ECHEC chargement\n", i + 1);
                ++fails;
                continue;
            }
            nk25d::Gameplay g;
            g.Init(&scene, board, i + 1);
            g.SetWindow(mFenW, mFenH);
            // Le bot réfléchit toutes les 0.25 s de jeu pendant le banc.
            g.SetBotPeriod(0.25f);
            if (mCasser == i + 1)
            {
                g.BreakLevel();
            }
            if (mLegacy)
            {
                g.SetLegacy(true);
            }
            // Nombre de cubes au départ (pour le rapport) et compteur de pas de simulation.
            const int cubes = board->Remaining();
            int steps = 0;
            // Le bot joue PAR LE CLIC : point d'appui (logique) -> pixels d'écran -> retour en logique -> OnTap, comme un doigt.
            const float kv = g.ViewScale();
            // Limite : 30 pas par seconde pendant 900 secondes (15 minutes de jeu), sinon le niveau est déclaré « temps dépassé ».
            while (g.GetState() == nk25d::Gameplay::PLAYING && steps < 30 * 900)
            {
                g.Tick(1.f / 30.f);
                ++steps;
                float lx;
                float ly;
                if (g.BotTick(1.f / 30.f, lx, ly))
                {
                    // mDecale (0 en temps normal) fait rater le clic avec --decaler-clic : contre-épreuve du banc.
                    g.OnTap((lx * kv + mDecale) / kv, ly);
                }
            }
            const nk25d::Stats& s = g.stats;
            // why : la liste des raisons d'échec, vide si le niveau passe.
            NkString why;
            // En cas de blocage, on décrit l'état des fourmis pour aider à comprendre pourquoi.
            NkString ants = g.GetState() == nk25d::Gameplay::PLAYING ? g.DescribeAnts() : NkString();
            if (g.GetState() == nk25d::Gameplay::LOST)
            {
                ants += g.DescribeState();
            }
            // En essai solo sur un niveau encore en cours : on laisse passer 15 pas (0.5 s) et on décrit les fourmis de nouveau.
            if (mSolo && g.GetState() == nk25d::Gameplay::PLAYING)
            {
                for (int k = 0; k < 15; ++k)
                {
                    g.Tick(1.f / 30.f);
                }
                ants += NkString("  APRES 0.5s :") + g.DescribeAnts();
            }
            // Critère 1 : la victoire.
            if (g.GetState() != nk25d::Gameplay::WON)
            {
                why += g.GetState() == nk25d::Gameplay::LOST ? " perdu(bloque)" : " temps-depasse";
            }
            // Critère 2 : le stock est équilibré, couleur par couleur.
            if (!s.balanced)
            {
                why += " stock-desequilibre";
            }
            // Critère 3 : les sorties de fourmis égalent la somme des numéros du stock.
            if (s.spawned != s.expected)
            {
                why += NkFormat(" sorties({0})!=somme({1})", s.spawned, s.expected);
            }
            // Critère 4 : chaque fourmi sortie est revenue au nid.
            if (s.returned != s.spawned)
            {
                why += NkFormat(" retours({0})!=sorties({1})", s.returned, s.spawned);
            }
            // Critère 5 : aucune sortie sans bloc réservé.
            if (s.unreserved)
            {
                why += NkFormat(" sorties-sans-reservation({0})", s.unreserved);
            }
            // Critère 6 : aucune fourmi ressortie deux fois.
            if (s.dupExit)
            {
                why += NkFormat(" fourmis-ressorties({0})", s.dupExit);
            }
            // Critère 7 : en cas de victoire, chaque bloc a été posé par un vrai clic (et non par un appel direct).
            if (g.GetState() == nk25d::Gameplay::WON && s.tapped != s.blocks)
            {
                why += NkFormat(" blocs-poses-sans-clic({0}/{1})", s.tapped, s.blocks);
            }
            // Critère 8 : aucun bloc réservé deux fois.
            if (s.dupReserve)
            {
                why += NkFormat(" bloc-reserve-deux-fois({0})", s.dupReserve);
            }
            // Critère 9 : aucun bloc réservé n'a disparu.
            if (s.lostBlock)
            {
                why += NkFormat(" bloc-reserve-disparu({0})", s.lostBlock);
            }
            // Le niveau passe si aucune raison d'échec n'a été notée.
            const bool ok = why.Empty();
            if (!ok)
            {
                ++fails;
            }
            rap += NkFormat("{0} : {1}  cubes {2}  sorties {3}  retours {4}  temps {5}s{6}\n", i + 1, ok ? "OK    " : "ECHEC ", cubes, s.spawned, s.returned, (int)g.PlayTime(), (why + ants).CStr());
        }
        rap += NkFormat("Resultat : {0} niveau(x) en echec sur {1}\n", fails, n);
        // Le rapport est écrit dans un fichier (--rapport, sinon foodhunt_banc.txt) : sans fenêtre, c'est la seule trace.
        NkFile::WriteAllText(mRapport.CStr(), rap.CStr());
        return fails;
    }

    // ---- L'état de l'application ------------------------------------------------------------------
    // La scène : les objets dessinés triés par profondeur (le plateau, les fourmis).
    nk25d::Scene mScene;
    // Le plateau du niveau en cours (appartient à la scène ; rechargé à chaque StartLevel).
    nk25d::Board* mBoard = nullptr;
    // La partie : fourmis, bot, interface, mise en page.
    nk25d::Gameplay mGame;
    // La bibliothèque des 25 niveaux embarqués.
    nk25d::LevelLibrary mLib;
    // Zone sûre de l'écran (encoche, barres), ou fausse encoche d'essai.
    NkSafeAreaInsets mSafe;
    // Vrai si --marges a imposé la zone sûre (le système ne doit alors pas la remplacer).
    bool mMargesForcees = false;
    // Taille de la fenêtre en pixels, mise à jour par OnLayout.
    float mWinW = kRefW;
    float mWinH = kRefH;
    // Secondes de bot à simuler avant d'afficher (--avance).
    float mAvance = 0.f;
    // Taille de fenêtre demandée (--fenetre) : le banc, sans fenêtre, s'en sert pour sa mise en page.
    float mFenW = kRefW;
    float mFenH = kRefH;
    // Décalage en pixels des clics du bot (0 normalement ; 300 avec --decaler-clic).
    float mDecale = 0.f;
    // Compteur de trames, et trames des deux clics d'essai (-1 = aucun).
    int mTick = 0;
    int mClicA = -1;
    int mClicB = -1;
    // Niveau en cours, niveau de départ (--niveau), niveau à casser (--casser-niveau ; 0 = aucun), tous de 0 à 24 sauf mCasser (1 à 25).
    int mLevel = 0;
    int mStart = 0;
    int mCasser = 0;
    // mSolo : on ne joue qu'un niveau ; mBot : le bot joue ; mBanc : mode banc ; mLegacy : ancienne conduite des fourmis.
    bool mSolo = false;
    bool mBot = kAutoPlay;
    bool mBanc = false;
    bool mLegacy = false;
    // Fichier où le banc écrit son rapport.
    NkString mRapport = NkString("foodhunt_banc.txt");
};

// Point d'entrée du programme : le moteur appelle nkmain, qui lance l'application FoodHunt.
int nkmain(const NkEntryState& state)
{
    return NkCanvasApp::Run<FoodHunt>(state);
}
