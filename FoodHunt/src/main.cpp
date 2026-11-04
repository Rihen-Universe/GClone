// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// main.cpp — Jeu complet : plateau de pixels 2.5D + stock + 5 slots + fourmis par couleur.
// Migré le 04/11/2026 : l'application hérite de NkCanvasGuiApp (NKCanvas) et dessine sur un NkGuiDrawList ; la
// police est celle, embarquée, de NKFont (plus aucun fichier Roboto) ; l'image est une NkImage.
#include "NKWindow/NKMain.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "Nk25DGame.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

class AntDemo : public NkCanvasGuiApp {
public:
    static constexpr float kW = 540.f, kH = 960.f;
    static constexpr bool  kAutoPlay = true;      // bot : la démo tourne seule (mettre false pour jouer au doigt ou à la souris)

    AntDemo() {
        Config().title = "Ants 2.5D"; Config().width = (int)kW; Config().height = (int)kH;
        Config().clearColor = NkColor2D(222, 184, 135, 255);
    }

protected:
    // Appelé par NkCanvasGuiApp::OnInit une fois le contexte graphique et les polices prêts.
    bool OnGuiInit() override {
        nk25d::GameFont() = FontBody();                                     // police embarquée de NKFont
        mBoard = mScene.Add<nk25d::Board>();
        NkImage img = MakeTestImage(160);
        if (!mBoard->LoadFromImage(img, /*blocs en largeur*/ 48, /*couleurs*/ 6, /*autoBg*/ false)) return false;
        mGame.level = 31;
        mGame.Init(&mScene, mBoard, kW, kH);       // place caméra, plateau, trou, stock, slots
        mGame.AutoPlay(kAutoPlay);
        return true;
    }
    void OnTick(float32 dt) override { mGame.Tick(dt); }                    // gère vitesse x1/x2/x3, collisions, fourmis
    void OnDraw(nkgui::NkGuiDrawList& dl) override {
        mGame.DrawBackground(dl);                  // plateau clair
        mScene.Render(dl);                         // blocs + fourmis triés par profondeur
        mGame.DrawUI(dl);                          // slots, stock, boutons, textes
    }

    // Entrée souris / doigt : un appui (phase NK_POINTER_DOWN) est un tap, en pixels client (0..kW, 0..kH).
    bool OnPointer(const NkPointer& ptr) override {
        if (ptr.phase != NkPointerPhase::NK_POINTER_DOWN) return false;
        mGame.OnTap(ptr.x, ptr.y);
        return true;
    }

private:
    static NkImage MakeTestImage(int N) {                                   // lapin procédural de test
        NkImage img = NkImage::Create((uint32)N, (uint32)N, NkImagePixelFormat::NK_RGBA32, 0u);
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            float u = (x + .5f) / N * 2 - 1, v = (y + .5f) / N * 2 - 1; uint8* p = img.RowPtr(y) + x * 4;
            p[0] = p[1] = p[2] = 0; p[3] = 0;
            auto in = [&](float cx, float cy, float rx, float ry) { float a = (u - cx) / rx, b = (v - cy) / ry; return a * a + b * b < 1; };
            auto set = [&](int r, int g, int b) { p[0] = (uint8)r; p[1] = (uint8)g; p[2] = (uint8)b; p[3] = 255; };
            if (in(-.45f, -.62f, .17f, .42f) || in(.45f, -.62f, .17f, .42f)) set(245, 245, 245);
            if (in(-.45f, -.6f, .09f, .3f) || in(.45f, -.6f, .09f, .3f)) set(240, 60, 170);
            if (in(0, .12f, .78f, .62f)) set(248, 248, 248);
            if (in(-.3f, 0, .08f, .1f) || in(.3f, 0, .08f, .1f)) set(40, 40, 50);
            if (in(0, .15f, .1f, .07f)) set(240, 60, 170);
            if (in(0, .62f, .55f, .2f)) set(40, 170, 240);
        }
        return img;
    }
    nk25d::Scene mScene; nk25d::Board* mBoard = nullptr; nk25d::Gameplay mGame;
};

int nkmain(const NkEntryState& state) { return NkCanvasApp::Run<AntDemo>(state); }
