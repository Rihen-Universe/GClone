// NkAntDemo.cpp — Jeu complet : plateau de pixels 2.5D + stock + 5 slots + fourmis par couleur.
// Intégration NKCanvas. Voir README.md pour les 4 points à adapter à votre version du moteur.
#include "NKWindow/NKMain.h"
#include "NKCanvas/App/NkCanvasApp.h"     // (1) chemin supposé
#include "NkPainter.h"
#include "Nk25DGame.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

class AntDemo : public NkCanvasApp {
public:
    static constexpr float kW = 540.f, kH = 960.f;
    static constexpr bool  kAutoPlay = true;      // bot : la démo tourne seule (mettre false quand OnPointer est branché)

    AntDemo() {
        Config().title = "Ants 2.5D"; Config().width = (int)kW; Config().height = (int)kH;
        Config().clearColor = NkColor2D{222, 184, 135, 255};
    }
    bool OnInit() override {
        mBoard = mScene.Add<nk25d::Board>();

        // (2) >>> Remplacer par votre image : NkImage -> (pixels RGBA, largeur, hauteur) -> LoadFromRGBA(...)
        const int N = 160; unsigned char* rgba = new unsigned char[N * N * 4];
        MakeTestImage(rgba, N);
        bool ok = mBoard->LoadFromRGBA(rgba, N, N, /*blocs en largeur*/ 48, /*couleurs*/ 6, /*autoBg*/ false);
        delete[] rgba;
        if (!ok) return false;

        mGame.level = 31;
        mGame.Init(&mScene, mBoard, kW, kH);       // place caméra, plateau, trou, stock, slots
        mGame.AutoPlay(kAutoPlay);

        mFontOk = mFont.LoadFromFile(*Target().GetRenderer(), "assets/Roboto-Regular.ttf");   // (3) police
        return true;
    }
    void OnUpdate(float32 dt) override { mGame.Tick(dt); }      // gère vitesse x1/x2/x3, collisions, fourmis
    void OnRender(NkRenderWindow& target) override {
        nk25d::NkPainter p(target, mFontOk ? &mFont : nullptr);
        mGame.DrawBackground(p);                   // plateau clair
        mScene.Render(p);                          // blocs + fourmis triés par profondeur
        mGame.DrawUI(p);                           // slots, stock, boutons, textes   (~NkPainter => Flush)
    }

    // (4) >>> Entrée souris / doigt : adapter ReadPointer à la structure NkPointer, puis définir NK25D_POINTER_READY.
    bool OnPointer(const NkPointer& ptr) override {
        float x = 0, y = 0; bool pressed = false;
        if (!ReadPointer(ptr, x, y, pressed)) return false;
        if (pressed) mGame.OnTap(x, y);            // coordonnées en pixels écran (0..kW, 0..kH)
        return true;
    }
private:
    static bool ReadPointer(const NkPointer& p, float& x, float& y, bool& pressed) {
#ifdef NK25D_POINTER_READY
        x = (float)p.x; y = (float)p.y; pressed = p.IsPressed();        // noms supposés : à corriger
        return true;
#else
        (void)p; (void)x; (void)y; (void)pressed; return false;
#endif
    }
    static void MakeTestImage(unsigned char* o, int N) {                // lapin procédural de test
        for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
            float u = (x + .5f) / N * 2 - 1, v = (y + .5f) / N * 2 - 1; unsigned char* p = o + (y * N + x) * 4;
            p[0] = p[1] = p[2] = 0; p[3] = 0;
            auto in = [&](float cx, float cy, float rx, float ry) { float a = (u - cx) / rx, b = (v - cy) / ry; return a * a + b * b < 1; };
            auto set = [&](int r, int g, int b) { p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b; p[3] = 255; };
            if (in(-.45f, -.62f, .17f, .42f) || in(.45f, -.62f, .17f, .42f)) set(245, 245, 245);
            if (in(-.45f, -.6f, .09f, .3f) || in(.45f, -.6f, .09f, .3f)) set(240, 60, 170);
            if (in(0, .12f, .78f, .62f)) set(248, 248, 248);
            if (in(-.3f, 0, .08f, .1f) || in(.3f, 0, .08f, .1f)) set(40, 40, 50);
            if (in(0, .15f, .1f, .07f)) set(240, 60, 170);
            if (in(0, .62f, .55f, .2f)) set(40, 170, 240);
        }
    }
    nk25d::Scene mScene; nk25d::Board* mBoard = nullptr; nk25d::Gameplay mGame;
    renderer::NkFont mFont; bool mFontOk = false;
};

int nkmain(const NkEntryState& state) { return NkCanvasApp::Run<AntDemo>(state); }