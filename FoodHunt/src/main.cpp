// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// main.cpp — FoodHunt : plateau de cubes 2.5D, stock en rayon de miel, paniers, fourmis par couleur.
// Migré le 04/11/2026 : l'application hérite de NkCanvasGuiApp (NKCanvas) et dessine sur un NkGuiDrawList ; la
// police est celle, embarquée, de NKFont. Réécrit le 05/11/2026 : les vingt-cinq niveaux sont des images embarquées
// (LevelLibrary), l'écran peut avoir n'importe quelle forme portrait, le bot est une option (--bot) et le banc
// `--bot --verifier-niveaux` joue les vingt-cinq niveaux sans fenêtre.
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

class FoodHunt : public NkCanvasGuiApp {
public:
    static constexpr float kRefW = 540.f, kRefH = 1000.f;       // taille LOGIQUE de référence (voir Nk25DGame.h)
    static constexpr bool  kAutoPlay = false;                   // le bot n'est plus le comportement par défaut (--bot)

    FoodHunt() {
        Config().title = "FoodHunt"; Config().width = 480; Config().height = 960;
        Config().clearColor = NkColor2D(150, 200, 100, 255);
    }

protected:
    // ---- Ligne de commande : le banc répond AVANT toute fenêtre ---------------------------------
    static bool StartsWith(const NkString& s, const char* p) { return s.StartsWith(p); }
    static int  ParseInt(const char* p, int def) { if (!p || !*p) return def; int v = 0; for (; *p >= '0' && *p <= '9'; ++p) v = v * 10 + (*p - '0'); return v; }

    NkOptional<int> OnCommandLine(const NkVector<NkString>& args) override {
        for (usize i = 0; i < args.Size(); ++i) {
            const NkString& a = args[i];
            if (a == "--bot") mBot = true;
            else if (a == "--verifier-niveaux") mBanc = true;
            else if (a == "--ancien-comportement") mLegacy = true;
            else if (StartsWith(a, "--clic-a=")) mClicA = ParseInt(a.CStr() + 9, -1);     // essai : un clic sur la tête de la colonne 1 à la trame n
            else if (StartsWith(a, "--clic-b=")) mClicB = ParseInt(a.CStr() + 9, -1);     // essai : un second clic (colonne 2) à la trame n
            else if (a == "--decaler-clic") mDecale = 300.f;                       // contre-épreuve : les clics du bot ratent leur cible
            else if (StartsWith(a, "--casser-niveau=")) mCasser = ParseInt(a.CStr() + 16, 0);
            else if (StartsWith(a, "--rapport=")) mRapport = NkString(a.CStr() + 10);
            else if (StartsWith(a, "--niveau=")) { mStart = math::NkClamp(ParseInt(a.CStr() + 9, 1), 1, 25) - 1; mSolo = true; }
            else if (StartsWith(a, "--avance=")) mAvance = (float)ParseInt(a.CStr() + 9, 0);
            else if (StartsWith(a, "--fenetre=")) { const char* q = a.CStr() + 10; int w = ParseInt(q, 480); while (*q >= '0' && *q <= '9') ++q; if (*q == 'x') ++q; int h = ParseInt(q, 960); Config().width = (uint32)w; Config().height = (uint32)h; mFenW = (float)w; mFenH = (float)h; }
        }
        if (mBanc && mBot) return NkOptional<int>(RunBench());
        return NkOptional<int>();
    }

    // La police suit l'échelle de l'écran : le texte est dessiné à ~24 px logiques puis mis à l'échelle par le même facteur.
    float32 TaillePoliceCorps(const NkLayoutInfo& lay) const noexcept override {
        float s = nk25d::NkView25D::ScaleFor((float)lay.width, (float)lay.height, kRefW, kRefH); float px = 24.f * s; return px < 12.f ? 12.f : (px > 72.f ? 72.f : px);
    }

    // Appelé par NkCanvasGuiApp::OnInit une fois le contexte graphique et les polices prêts.
    bool OnGuiInit() override {
        nk25d::GameFont() = FontBody();                                     // police embarquée de NKFont
        if (mLib.LoadEmbedded() <= 0) return false;
        mLevel = math::NkClamp(mStart, 0, mLib.Count() - 1);
        if (!StartLevel(mLevel)) return false;
        if (mAvance > 0.f) {                                                // essais : avancer la partie (le bot CLIQUE) avant la capture
            mGame.SetBotPeriod(0.3f);
            for (float t = 0.f; t < mAvance && mGame.GetState() == nk25d::Gameplay::PLAYING; t += 1.f / 30.f) { mGame.Tick(1.f / 30.f); BotStep(1.f / 30.f); }
        }
        return true;
    }
    void OnTick(float32 dt) override {
        ++mTick;
        { float lx, ly; int col = mTick == mClicA ? 1 : (mTick == mClicB ? 2 : -1);
          if (col >= 0 && mGame.CellPoint(col, 0, lx, ly)) Tap(lx * mGame.ViewScale(), ly * mGame.ViewScale()); }
        mGame.Tick(dt);
        if (mBot) BotStep(dt * mGame.TimeScale());
        if (mBot && mGame.GetState() != nk25d::Gameplay::PLAYING && mGame.EndTime() > 2.5f)          // le bot enchaîne les niveaux
            StartLevel(mGame.GetState() == nk25d::Gameplay::WON ? (mLevel + 1) % mLib.Count() : mLevel);
    }

    // Tout le dessin se fait en coordonnées LOGIQUES, puis les sommets sont mis à l'échelle de l'écran d'un seul coup :
    // le jeu n'a ainsi qu'UNE mise en page à connaître (voir Gameplay::SetWindow), quelle que soit la forme de l'écran.
    void OnDraw(nkgui::NkGuiDrawList& dl) override {
        // Deux espaces de coordonnées : la scène (cubes, fourmis) passe par NkView25D::Project et sort DÉJÀ en pixels ;
        // le fond et l'interface sont dessinés en coordonnées logiques du plan de la vue puis mis à l'échelle (k).
        const float k = mGame.ViewScale();
        usize v0 = dl.vtx.Size();
        mGame.DrawBackground(dl);                  // herbe, assiette, nappe
        Scale(dl, v0, k);
        mScene.Render(dl);                         // cubes + fourmis triés par profondeur (pixels)
        v0 = dl.vtx.Size();
        mGame.DrawEffects(dl);                     // miettes, confettis
        mGame.DrawUI(dl);                          // paniers, rayon de miel, boutons, écrans de fin
        Scale(dl, v0, k);
    }

    // Redimensionnement, rotation : la forme de l'écran change, le monde du jeu ne bouge pas.
    void OnLayout(const NkLayoutInfo& lay) override {
        NkCanvasGuiApp::OnLayout(lay);
        mWinW = (float)lay.width; mWinH = (float)lay.height;
        mGame.SetWindow(mWinW, mWinH);
    }

    // Entrée souris / doigt : un appui est un tap, en pixels client -> coordonnées logiques (division par l'échelle).
    bool OnPointer(const NkPointer& ptr) override {
        if (ptr.phase != NkPointerPhase::NK_POINTER_DOWN) return false;
        Tap(ptr.x, ptr.y);
        return true;
    }

private:
    // LE chemin du clic : pixels d'écran -> coordonnées logiques (division par l'échelle de la vue) -> Gameplay::OnTap.
    // La souris, le doigt ET le bot passent tous par ici : le bot ne pose plus un bloc par un appel direct.
    void Tap(float px, float py) {
        const float k = mGame.ViewScale();
        nk25d::Gameplay::Action a = mGame.OnTap(px / k, py / k);
        if (a == nk25d::Gameplay::ACT_NEXT) StartLevel((mLevel + 1) % mLib.Count());
        else if (a == nk25d::Gameplay::ACT_RESTART) StartLevel(mLevel);
    }
    // Le bot « appuie » : il rend un point de l'écran logique, on le convertit en pixels (comme un vrai doigt) et on tape.
    void BotStep(float dt) {
        float lx, ly;
        if (mGame.GetState() == nk25d::Gameplay::PLAYING && mGame.BotTick(dt, lx, ly)) Tap(lx * mGame.ViewScale() + mDecale, ly * mGame.ViewScale());
    }
    // Met à l'échelle les sommets ajoutés depuis v0 (le plan logique -> les pixels).
    static void Scale(nkgui::NkGuiDrawList& dl, usize v0, float k) { for (usize i = v0; i < dl.vtx.Size(); ++i) { dl.vtx[i].pos.x *= k; dl.vtx[i].pos.y *= k; } }

    // Charge le niveau i : la scène est vidée, un plateau neuf est posé, le jeu repart de zéro.
    bool StartLevel(int i) {
        mLevel = i; mScene.Clear(); mBoard = mScene.Add<nk25d::Board>();
        if (!mLib.LoadLevel(i, *mBoard)) return false;
        mGame.SetWindow(mWinW, mWinH);
        mGame.Init(&mScene, mBoard, i + 1);
        if (mCasser == i + 1) mGame.BreakLevel();
        if (mLegacy) mGame.SetLegacy(true);
        return true;
    }

    // ---- Le BANC : joue les 25 niveaux en pur calcul (aucune fenêtre, aucun GPU) -------------------------
    // Pour chaque niveau : pas fixe de 1/30 s, au plus 15 minutes de jeu, le bot joue. Un niveau PASSE si :
    //   victoire ; somme des numéros du stock = nombre de cubes, couleur par couleur ; sorties = somme des numéros ;
    //   retours au nid = sorties ; aucune sortie sans bloc réservé ; aucune fourmi ressortie ; aucun bloc réservé deux fois ;
    //   aucun bloc réservé disparu. Le code de sortie est le nombre de niveaux en échec.
    int RunBench() {
        nk25d::LevelLibrary lib; int n = lib.LoadEmbedded();
        NkString rap = NkFormat("FoodHunt, banc des niveaux ({0} niveaux){1}{2}\n", n, mLegacy ? " [ancien comportement]" : "", mCasser > 0 ? " [niveau cassé]" : "");
        int fails = 0;
        // Épreuve de la VUE : avec une vue NEUTRE (échelle 1), NkView25D::Project doit rendre exactement les mêmes points que
        // l'ancienne formule de Camera ; Unproject doit défaire Project ; et une vue NON neutre doit, elle, différer (contre-épreuve :
        // sans elle, un Project qui ignorerait la vue passerait aussi l'épreuve).
        { nk25d::NkView25D v; nk25d::CameraAncienne old; old.origin = nkentseu::math::NkVec2f(13.f, 27.f); old.tilt = v.tilt = 0.85f;
          v.Neutral(540.f, 1000.f); v.center = v.size * .5f - old.origin;
          nk25d::NkView25D w = v; w.window = nkentseu::math::NkVec2f(1080.f, 2000.f);
          int bad = 0, badRound = 0, wDiffers = 0, total = 0;
          for (float x = -50.f; x <= 600.f; x += 37.f) for (float y = -50.f; y <= 700.f; y += 41.f) for (float z = 0.f; z <= 30.f; z += 7.5f) {
              nkentseu::math::NkVec2f a2 = v.Project(x, y, z), b2 = old.Project(x, y, z), c2 = w.Project(x, y, z), s1 = v.ShadowPos(x, y, z), s2 = old.ShadowPos(x, y, z);
              ++total;
              if (nk25d::NkAbs(a2.x - b2.x) > 1e-3f || nk25d::NkAbs(a2.y - b2.y) > 1e-3f || nk25d::NkAbs(s1.x - s2.x) > 1e-3f || nk25d::NkAbs(s1.y - s2.y) > 1e-3f || nk25d::NkAbs(v.Depth(x, y, z) - old.Depth(x, y, z)) > 1e-3f) ++bad;
              nkentseu::math::NkVec2f g = v.Unproject(v.Project(x, y, 0.f).x, v.Project(x, y, 0.f).y); if (nk25d::NkAbs(g.x - x) > 1e-2f || nk25d::NkAbs(g.y - y) > 1e-2f) ++badRound;
              if (nk25d::NkAbs(c2.x - b2.x) > 1.f || nk25d::NkAbs(c2.y - b2.y) > 1.f) ++wDiffers;
          }
          const bool okVue = bad == 0 && badRound == 0 && wDiffers > total / 2;
          if (!okVue) ++fails;
          rap += NkFormat("Vue neutre : {0}  points {1}  ecarts {2}  aller-retour {3}  vue-non-neutre-differe {4}\n", okVue ? "OK" : "ECHEC", total, bad, badRound, wDiffers);
        }
        for (int i = 0; i < n; ++i) {
            if (mSolo && i != mStart) continue;
            nk25d::Scene scene; nk25d::Board* board = scene.Add<nk25d::Board>();
            if (!lib.LoadLevel(i, *board)) { rap += NkFormat("{0} : ECHEC chargement\n", i + 1); ++fails; continue; }
            nk25d::Gameplay g; g.Init(&scene, board, i + 1); g.SetWindow(mFenW, mFenH); g.SetBotPeriod(0.25f);
            if (mCasser == i + 1) g.BreakLevel();
            if (mLegacy) g.SetLegacy(true);
            const int cubes = board->Remaining(); int steps = 0;
            // Le bot joue PAR LE CLIC : point d'appui (logique) -> pixels d'écran -> retour en logique -> OnTap, comme un doigt.
            const float kv = g.ViewScale();
            while (g.GetState() == nk25d::Gameplay::PLAYING && steps < 30 * 900) {
                g.Tick(1.f / 30.f); ++steps; float lx, ly;
                if (g.BotTick(1.f / 30.f, lx, ly)) g.OnTap((lx * kv + mDecale) / kv, ly);
            }
            const nk25d::Stats& s = g.stats; NkString why;
            NkString ants = g.GetState() == nk25d::Gameplay::PLAYING ? g.DescribeAnts() : NkString();
            if (g.GetState() == nk25d::Gameplay::LOST) ants += g.DescribeState();
            if (mSolo && g.GetState() == nk25d::Gameplay::PLAYING) { for (int k = 0; k < 15; ++k) g.Tick(1.f / 30.f); ants += NkString("  APRES 0.5s :") + g.DescribeAnts(); }
            if (g.GetState() != nk25d::Gameplay::WON) why += g.GetState() == nk25d::Gameplay::LOST ? " perdu(bloque)" : " temps-depasse";
            if (!s.balanced) why += " stock-desequilibre";
            if (s.spawned != s.expected) why += NkFormat(" sorties({0})!=somme({1})", s.spawned, s.expected);
            if (s.returned != s.spawned) why += NkFormat(" retours({0})!=sorties({1})", s.returned, s.spawned);
            if (s.unreserved) why += NkFormat(" sorties-sans-reservation({0})", s.unreserved);
            if (s.dupExit) why += NkFormat(" fourmis-ressorties({0})", s.dupExit);
            if (g.GetState() == nk25d::Gameplay::WON && s.tapped != s.blocks) why += NkFormat(" blocs-poses-sans-clic({0}/{1})", s.tapped, s.blocks);
            if (s.dupReserve) why += NkFormat(" bloc-reserve-deux-fois({0})", s.dupReserve);
            if (s.lostBlock) why += NkFormat(" bloc-reserve-disparu({0})", s.lostBlock);
            const bool ok = why.Empty(); if (!ok) ++fails;
            rap += NkFormat("{0} : {1}  cubes {2}  sorties {3}  retours {4}  temps {5}s{6}\n", i + 1, ok ? "OK    " : "ECHEC ", cubes, s.spawned, s.returned, (int)g.PlayTime(), (why + ants).CStr());
        }
        rap += NkFormat("Resultat : {0} niveau(x) en echec sur {1}\n", fails, n);
        NkFile::WriteAllText(mRapport.CStr(), rap.CStr());
        return fails;
    }

    nk25d::Scene mScene; nk25d::Board* mBoard = nullptr; nk25d::Gameplay mGame; nk25d::LevelLibrary mLib;
    float mWinW = kRefW, mWinH = kRefH, mAvance = 0.f, mFenW = kRefW, mFenH = kRefH, mDecale = 0.f; int mTick = 0, mClicA = -1, mClicB = -1;
    int mLevel = 0, mStart = 0, mCasser = 0; bool mSolo = false, mBot = kAutoPlay, mBanc = false, mLegacy = false;
    NkString mRapport = NkString("foodhunt_banc.txt");
};

int nkmain(const NkEntryState& state) { return NkCanvasApp::Run<FoodHunt>(state); }
