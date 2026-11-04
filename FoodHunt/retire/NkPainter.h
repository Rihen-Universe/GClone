// RETIRE le 04/11/2026 : adaptateur IPainter -> NKCanvas devenu inutile, le jeu dessine maintenant directement sur nkgui::NkGuiDrawList (NKGui) servi par NkCanvasGuiApp. Gardé ici, non compilé (déplacé, pas supprimé).
// NkPainter.hpp — Adaptateur IPainter -> NKCanvas, SANS STL.
// Tout est accumulé en TRIANGLES (NK_TRIANGLES uniquement, pas de fan) dans un buffer
// fixe et envoyé en UN SEUL target.Draw() par flush -> très peu d'appels GPU.
#pragma once
#include "Nk25D.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Resources/NkFont.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"   // chemin supposé

namespace nk25d {
using namespace nkentseu;
using namespace nkentseu::renderer;

class NkPainter : public IPainter {
public:
    enum { CAP = 24576 };                                     // multiple de 3
    NkPainter(NkRenderWindow& t, renderer::NkFont* font = nullptr) : mT(t), mFont(font) { mBuf = new NkVertex[CAP]; }
    ~NkPainter() { Flush(); delete[] mBuf; }
    NkPainter(const NkPainter&) = delete; NkPainter& operator=(const NkPainter&) = delete;

    void Flush() {
        if (mN) { mT.Draw(mBuf, (uint32)mN, NkPrimitiveType::NK_TRIANGLES); mN = 0; }
    }

    void FillPolygon(const Vec2* p, int n, Col c) override {  // convexe : triangles (p0, pi, pi+1)
        for (int i = 1; i + 1 < n; ++i) { Tri(p[0], p[i], p[i + 1], c); }
    }
    void FillEllipse(Vec2 c, float rx, float ry, Col col) override {
        const int N = 16;
        Vec2 prev{c.x + rx, c.y};
        for (int i = 1; i <= N; ++i) {
            float a = 2 * PI * i / N; Vec2 cur{c.x + m::Cos(a) * rx, c.y + m::Sin(a) * ry};
            Tri(c, prev, cur, col); prev = cur;
        }
    }
    void DrawLine(Vec2 a, Vec2 b, float w, Col c) override {
        float dx = b.x - a.x, dy = b.y - a.y, l = m::Hypot(dx, dy);
        if (l < 1e-4f) return;
        float nx = -dy / l * w * .5f, ny = dx / l * w * .5f;
        Vec2 q0{a.x + nx, a.y + ny}, q1{b.x + nx, b.y + ny}, q2{b.x - nx, b.y - ny}, q3{a.x - nx, a.y - ny};
        Tri(q0, q1, q2, c); Tri(q0, q2, q3, c);
    }
    void DrawText(Vec2 c, const char* s, float size, Col col) override {
        if (!mFont) return;
        Flush();                                               // garde l'ordre de dessin avec le texte
        int len = 0; while (s[len]) ++len;
        NkText t(*mFont, s, (uint32)size);
        t.SetFillColor(NkColor2D{col.r, col.g, col.b, col.a});
        t.SetPosition({c.x - 0.30f * size * len, c.y - 0.62f * size});
        mT.Draw(t);
    }
private:
    void Tri(Vec2 a, Vec2 b, Vec2 c, Col k) {
        if (mN + 3 > CAP) Flush();
        mBuf[mN++] = V(a, k); mBuf[mN++] = V(b, k); mBuf[mN++] = V(c, k);
    }
    static NkVertex V(Vec2 p, Col c) { return NkVertex{p.x, p.y, 0.f, 0.f, c.r, c.g, c.b, c.a}; }
    NkRenderWindow& mT; renderer::NkFont* mFont; NkVertex* mBuf; int mN = 0;
};
} // namespace nk25d