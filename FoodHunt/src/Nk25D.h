// Nk25D.hpp — Fourmis + boîtes 2.5D procédurales, collisions, portage de blocs.
// SANS STL (pas de vector/function/algorithm/memory). Seules dépendances : math.h/stdlib.h (C).
//   -DNK25D_USE_NKVECTOR : utilise nkentseu::NkVector (sinon mini-tableau interne)
//   -DNK25D_USE_NKMATH   : utilise nkentseu::math::NkSqrt (voir namespace m pour brancher le reste)
#pragma once
#include <stdlib.h>
#include <math.h>
#ifdef NK25D_USE_NKMATH
  #include "NKMath/NKMath.h"
#endif
#ifdef NK25D_USE_NKVECTOR
  #include "NKContainers/Sequential/NkVector.h"      // chemin supposé
#endif

namespace nk25d {

constexpr float PI = 3.14159265f;

// ---- Couche math : un seul endroit à brancher sur NKMath --------------------
namespace m {
    inline float Sin(float v)  { return sinf(v); }
    inline float Cos(float v)  { return cosf(v); }
    inline float Atan2(float y, float x) { return atan2f(y, x); }
    inline float Exp(float v)  { return expf(v); }
    inline float Sqrt(float v) {
#ifdef NK25D_USE_NKMATH
        return nkentseu::math::NkSqrt(v);
#else
        return sqrtf(v);
#endif
    }
    inline float Hypot(float a, float b) { return Sqrt(a * a + b * b); }
    template <class T> inline T Min(T a, T b) { return a < b ? a : b; }
    template <class T> inline T Max(T a, T b) { return a > b ? a : b; }
    template <class T> inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float Wrap(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
}

// ---- Tableau dynamique ------------------------------------------------------
#ifdef NK25D_USE_NKVECTOR
template <class T> using Array = nkentseu::NkVector<T>;
#else
template <class T> class Array {
public:
    Array() = default; ~Array() { free(d_); }
    Array(const Array&) = delete; Array& operator=(const Array&) = delete;
    void PushBack(const T& v) { if (n_ == c_) { c_ = c_ ? c_ * 2 : 8; d_ = (T*)realloc(d_, c_ * sizeof(T)); } d_[n_++] = v; }
    int  Size() const { return n_; }
    void Clear() { n_ = 0; }
    T& operator[](int i) { return d_[i]; }
private: T* d_ = nullptr; int n_ = 0, c_ = 0;
};
#endif

struct Vec2 { float x = 0, y = 0; };
struct Col  { unsigned char r, g, b, a = 255; };

inline Col Shade(Col c, float f) {
    auto k = [&](unsigned char v) { return (unsigned char)m::Clamp(v * f, 0.f, 255.f); };
    return { k(c.r), k(c.g), k(c.b), c.a };
}

// ---- Interface de rendu (adapter à NKCanvas : voir NkPainter.hpp) ------------
struct IPainter {
    virtual ~IPainter() {}
    virtual void FillPolygon(const Vec2* pts, int n, Col c) = 0;      // convexe
    virtual void FillEllipse(Vec2 center, float rx, float ry, Col c) = 0;
    virtual void DrawLine(Vec2 a, Vec2 b, float width, Col c) = 0;
    virtual void DrawText(Vec2 center, const char* text, float size, Col c) = 0;
};

// ---- Caméra : écran = (x, y*tilt - z). Profondeur de vue = y + z*tilt --------
// (plus haut = plus près de la caméra, car on regarde d'en haut)
struct Camera {
    Vec2  origin{0, 0};
    float tilt = 0.65f;
    Vec2  lightDir{0.55f, 0.30f};
    Vec2 Project(float x, float y, float z) const { return { origin.x + x, origin.y + y * tilt - z }; }
    Vec2 ShadowPos(float x, float y, float z) const { return Project(x + lightDir.x * z, y + lightDir.y * z, 0); }
    float Depth(float, float y, float z) const { return y + z * tilt; }
};

inline void DrawSphere(IPainter& p, const Camera& cam, float x, float y, float z, float r, Col base) {
    Vec2 c = cam.Project(x, y, z);
    p.FillEllipse(c, r, r, Shade(base, 0.60f));
    p.FillEllipse({c.x - r * .10f, c.y - r * .12f}, r * .88f, r * .88f, base);
    p.FillEllipse({c.x - r * .30f, c.y - r * .35f}, r * .40f, r * .32f, Shade(base, 1.35f));
}

// ---- File de rendu : commandes POD (pas de std::function) --------------------
// Pièce dessinable personnalisée (ex : cellules du plateau)
struct ICustomDraw { virtual void DrawPiece(IPainter& p, const Camera& cam, int idx) const = 0; virtual ~ICustomDraw() {} };

enum CmdKind { CMD_LINE, CMD_ELLIPSE, CMD_SPHERE, CMD_BOX, CMD_TAPER, CMD_CUSTOM };
struct DrawCmd {
    float depth; int kind;
    Vec2 p0, p1; float a, b; Col col; const void* ptr;
};
struct RenderQueue {
    DrawCmd* cmds; int n, cap;
    void Push(const DrawCmd& c) { if (n < cap) cmds[n++] = c; }
    void Line(float dp, Vec2 a, Vec2 b, float w, Col c)    { Push({dp, CMD_LINE, a, b, w, 0, c, nullptr}); }
    void Taper(float dp, Vec2 a, Vec2 b, float w0, float w1, Col c) { Push({dp, CMD_TAPER, a, b, w0, w1, c, nullptr}); }
    void Ellipse(float dp, Vec2 c0, float rx, float ry, Col c) { Push({dp, CMD_ELLIPSE, c0, {}, rx, ry, c, nullptr}); }
    void Sphere(float dp, float x, float y, float z, float r, Col c) { Push({dp, CMD_SPHERE, {x, y}, {}, z, r, c, nullptr}); }
    void Custom(float dp, const void* iface, int idx) { Push({dp, CMD_CUSTOM, {}, {}, (float)idx, 0, {0,0,0,0}, iface}); }
    void BoxCmd(float dp, const void* b) { Push({dp, CMD_BOX, {}, {}, 0, 0, {0,0,0,0}, b}); }
};

enum EntityKind { KIND_ANT, KIND_BOX, KIND_BOARD, KIND_HOLE };
struct Entity {
    float x = 0, y = 0; int kind = 0; bool dead = false;
    virtual ~Entity() {}
    // Obstacle : repousse un cercle hors de l'entité ; renvoie true + normale si contact
    virtual bool ResolveCircle(float&, float&, float, float&, float&) { return false; }
    virtual void Update(float dt) = 0;
    virtual void LateUpdate() {}
    virtual void DrawShadow(IPainter&, const Camera&) const = 0;
    virtual void Emit(RenderQueue&, const Camera&) const = 0;
};

inline void IntToStr(int v, char* o) {
    char t[12]; int n = 0, neg = v < 0; if (neg) v = -v;
    do { t[n++] = char('0' + v % 10); v /= 10; } while (v);
    int k = 0; if (neg) o[k++] = '-'; while (n) o[k++] = t[--n]; o[k] = 0;
}

// Segment effilé (épais en a, fin en b) : base des pattes stylisées
inline void DrawTaper(IPainter& p, Vec2 a, Vec2 b, float w0, float w1, Col c) {
    float dx = b.x - a.x, dy = b.y - a.y, l = m::Hypot(dx, dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * .5f, ny = dx / l * .5f;
    Vec2 q[4] = { {a.x + nx * w0, a.y + ny * w0}, {b.x + nx * w1, b.y + ny * w1},
                  {b.x - nx * w1, b.y - ny * w1}, {a.x - nx * w0, a.y - ny * w0} };
    p.FillPolygon(q, 4, c);
}

class Ant;

// =============================================================================
//  BOX : bloc 3D orientable, déplaçable, portable (se réduit quand porté)
// =============================================================================
class Box : public Entity {
public:
    float w = 56, d = 44, h = 22, yaw = 0;
    int   value = 0;
    Col   color{80, 200, 240, 255};
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

    // Point de l'empreinte le plus proche de (px,py) (pour collisions / ramassage)
    void Closest(float px, float py, float& cx, float& cy) const {
        float cs = m::Cos(yaw), sn = m::Sin(yaw), dx = px - x, dy = py - y;
        float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;
        float hx = w * sc_ * .5f, hy = d * sc_ * .5f;
        float qx = m::Clamp(lx, -hx, hx), qy = m::Clamp(ly, -hy, hy);
        cx = x + qx * cs - qy * sn; cy = y + qx * sn + qy * cs;
    }

    void Update(float dt) override {
        sc_ += (scT_ - sc_) * (1 - m::Exp(-10 * dt));
        if (!carried) {
            if (moving_) {
                float dx = tx_ - x, dy = ty_ - y, dist = m::Hypot(dx, dy), k = 1 - m::Exp(-12 * dt);
                x += dx * k; y += dy * k;
                hop_ = m::Min(dist * 0.25f, 14.f);
                if (faceMotion && dist > 6) yawT_ = m::Atan2(dy, dx);
                if (dist < .3f) { x = tx_; y = ty_; moving_ = false; hop_ = 0; }
            }
            yaw += m::Wrap(yawT_ - yaw) * (1 - m::Exp(-14 * dt));
        }
        vel_ += (-180 * squash_ - 14 * vel_) * dt;
        squash_ += vel_ * dt;
    }

    void DrawShadow(IPainter& p, const Camera& cam) const override {
        float top = h * sc_ + hop_ + lift;
        Vec2 pts[8];
        for (int i = 0; i < 4; ++i) {
            Vec2 c = Corner(i);
            pts[i] = cam.Project(c.x, c.y, 0); pts[i + 4] = cam.ShadowPos(c.x, c.y, top);
        }
        Vec2 hull[16]; int n = Hull(pts, 8, hull);
        p.FillPolygon(hull, n, {0, 0, 0, carried ? (unsigned char)35 : (unsigned char)55});
    }

    void Emit(RenderQueue& q, const Camera& cam) const override {
        float hh = m::Max(4.f, h * sc_);
        q.BoxCmd(cam.Depth(x, y, hop_ + lift + hh), this);
    }

    void DrawBody(IPainter& p, const Camera& cam) const {
        float z0 = hop_ + lift, hh = m::Max(4.f * sc_, h * sc_ * (1 - squash_ * 0.012f)), z1 = z0 + hh;
        Vec2 c[4]; for (int i = 0; i < 4; ++i) c[i] = Corner(i);
        float lx = -cam.lightDir.x, ly = -cam.lightDir.y, ll = m::Hypot(lx, ly); lx /= ll; ly /= ll;
        for (int i = 0; i < 4; ++i) {
            Vec2 a = c[i], b = c[(i + 1) & 3];
            float nx = b.y - a.y, ny = -(b.x - a.x), nl = m::Hypot(nx, ny); nx /= nl; ny /= nl;
            if (ny <= 0.001f) continue;
            float lit = m::Clamp(0.5f + 0.35f * (nx * lx + ny * ly), 0.4f, 0.95f);
            Vec2 q[4] = { cam.Project(a.x, a.y, z0), cam.Project(b.x, b.y, z0), cam.Project(b.x, b.y, z1), cam.Project(a.x, a.y, z1) };
            p.FillPolygon(q, 4, Shade(color, lit));
            p.DrawLine(q[3], q[2], 1.5f * sc_, Shade(color, 1.35f));
        }
        Vec2 T[4], I[4]; float k = 1 - 8.f / m::Min(w, d);
        for (int i = 0; i < 4; ++i) {
            T[i] = cam.Project(c[i].x, c[i].y, z1);
            I[i] = cam.Project(x + (c[i].x - x) * k, y + (c[i].y - y) * k, z1);
        }
        p.FillPolygon(T, 4, Shade(color, 1.30f));
        p.FillPolygon(I, 4, Shade(color, 1.12f));
        if (showText && sc_ > 0.6f) {
            char buf[16]; if (mystery) { buf[0] = '?'; buf[1] = 0; } else IntToStr(value, buf);
            Vec2 t = cam.Project(x, y, z1);
            p.DrawText({t.x + 1, t.y + 2}, buf, 22 * sc_, {0, 0, 0, 90});
            p.DrawText(t, buf, 22 * sc_, {255, 255, 255, 255});
        }
    }

private:
    Vec2 Corner(int i) const {
        static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
        float lx = sx[i] * w * sc_ * .5f, ly = sy[i] * d * sc_ * .5f, cs = m::Cos(yaw), sn = m::Sin(yaw);
        return { x + lx * cs - ly * sn, y + lx * sn + ly * cs };
    }
    static int Hull(Vec2* P, int n, Vec2* out) {
        for (int i = 1; i < n; ++i) { Vec2 v = P[i]; int j = i - 1;           // tri par insertion
            while (j >= 0 && (P[j].x > v.x || (P[j].x == v.x && P[j].y > v.y))) { P[j + 1] = P[j]; --j; } P[j + 1] = v; }
        auto cr = [](Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
        Vec2 H[16]; int k = 0;
        for (int i = 0; i < n; ++i) { while (k >= 2 && cr(H[k-2], H[k-1], P[i]) <= 0) --k; H[k++] = P[i]; }
        for (int i = n - 2, t = k + 1; i >= 0; --i) { while (k >= t && cr(H[k-2], H[k-1], P[i]) <= 0) --k; H[k++] = P[i]; }
        for (int i = 0; i < k - 1; ++i) out[i] = H[i];
        return k - 1;
    }
    float tx_ = 0, ty_ = 0, hop_ = 0, squash_ = 0, vel_ = 0, yawT_ = 0, sc_ = 1, scT_ = 1;
    bool  moving_ = false;
};

// =============================================================================
//  ANT : fourmi procédurale, steering, évitement, ramasse/porte/dépose un Box
// =============================================================================
class Ant : public Entity {
public:
    Col   color{225, 40, 60, 255};
    float speed = 70, heading = 0, scale = 1.0f;
    int   colorIdx = -1, task = -1, owner = -1;      // jeu : couleur de la fourmi / cellule visée
    float arriveRadius = 3;              // distance à laquelle la cible est considérée atteinte
    bool  eatOnArrive = false;           // détruit le bloc porté à l'arrivée (jeu)
    int   legPairs = 3;                  // 3 = fourmi (6 pattes) ; 4 = 8 pattes

    Ant() { kind = KIND_ANT; rng_ = (unsigned)((size_t)this >> 4) * 2654435761u | 1u; }
    float Radius() const { return 8.f * scale; }
    void SetTarget(float tx, float ty) { tx_ = tx; ty_ = ty; hasTarget_ = true; nwp_ = iwp_ = 0; }
    // Chemin : points de passage puis cible finale (calculé par Board::FindPath*)
    void SetPath(const Vec2* p, int n, float fx, float fy) {
        nwp_ = n > 40 ? 40 : n; for (int i = 0; i < nwp_; ++i) wp_[i] = p[i];
        iwp_ = 0; tx_ = fx; ty_ = fy; hasTarget_ = true;
    }
    bool Arrived() const { return !hasTarget_; }
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

    void Update(float dt) override {
        t_ += dt;
        if (state_ == 1) {
            if (!box_ || box_->carried) { state_ = 0; box_ = nullptr; hasTarget_ = false; }
            else {
                float cx, cy; box_->Closest(x, y, cx, cy);
                if (m::Hypot(cx - x, cy - y) <= Radius() + 3.f) {
                    box_->SetCarried(true); box_->carrier = this; state_ = 2; SetTarget(dropX_, dropY_); ResetStuck();
                } else SetTarget(box_->x, box_->y);
            }
        } else if (state_ == 2 && !hasTarget_) {
            if (eatOnArrive) { box_->SetCarried(false); box_->dead = true; box_ = nullptr; state_ = 0; ResetStuck(); }
            else {
                float fx = m::Cos(heading), fy = m::Sin(heading);
                box_->SetCarried(false); box_->x = x + fx * 30 * scale; box_->y = y + fy * 30 * scale; box_->SetYaw(heading);
                box_ = nullptr; state_ = 0;
            }
        }
        if (hasTarget_) {
            while (iwp_ < nwp_ && m::Hypot(wp_[iwp_].x - x, wp_[iwp_].y - y) < 7.f) ++iwp_;
            float fd = m::Hypot(tx_ - x, ty_ - y);
            if (iwp_ >= nwp_ && fd < arriveRadius) { hasTarget_ = false; stuckAcc_ = 0; }
            else {
                float gx = iwp_ < nwp_ ? wp_[iwp_].x : tx_, gy = iwp_ < nwp_ ? wp_[iwp_].y : ty_;
                float dx = gx - x, dy = gy - y, dist = m::Max(1e-3f, m::Hypot(dx, dy));
                // Détection d'impasse : peu de progrès sur 0.5 s -> manoeuvre de dégagement (latérale + recul)
                stuckAcc_ += dt;
                if (stuckAcc_ >= 0.5f) {
                    if (esc_ <= 0 && m::Hypot(x - sx_, y - sy_) < speed * 0.08f) { esc_ = 0.9f; escSign_ = (Rnd() & 1) ? 1.f : -1.f; ++stuck_; }
                    sx_ = x; sy_ = y; stuckAcc_ = 0;
                }
                float vx = dx / dist, vy = dy / dist;
                if (esc_ > 0) { esc_ -= dt; float ex = -vy * escSign_ - vx * 0.35f, ey = vx * escSign_ - vy * 0.35f; vx = ex; vy = ey; }
                else if (avT_ > 0) { float k = 1.6f * (avT_ / 0.4f); vx += avX_ * k; vy += avY_ * k; avT_ -= dt; }
                float diff = m::Wrap(m::Atan2(vy, vx) - heading);
                heading = m::Wrap(heading + m::Clamp(diff, -8 * dt, 8 * dt));
                float v = speed * m::Max(0.2f, m::Cos(diff));
                x += m::Cos(heading) * v * dt; y += m::Sin(heading) * v * dt;
                phase_ += v * dt * 0.28f;
            }
        } else stuckAcc_ = 0;
        LateUpdate();
    }
    void LateUpdate() override {                                              // le bloc suit APRÈS les collisions
        if (state_ == 2 && box_) {
            box_->x = x + m::Cos(heading) * scale; box_->y = y + m::Sin(heading) * scale;
            box_->lift = 14.f * scale; box_->SetYaw(heading);
        }
    }

    void DrawShadow(IPainter& p, const Camera& cam) const override {
        float s = scale, fx = m::Cos(heading), fy = m::Sin(heading);
        const float off[2] = {-6.5f, 5.5f}, rad[2] = {7.5f, 8.5f};
        for (int i = 0; i < 2; ++i) {
            Vec2 c = cam.ShadowPos(x + fx * off[i] * s, y + fy * off[i] * s, 6 * s);
            p.FillEllipse(c, rad[i] * s, rad[i] * s * cam.tilt * 1.1f, {0, 0, 0, 55});
        }
    }

    // Style "chibi" : grosse tête + abdomen, oreilles-antennes épaisses, pattes courtes, gros yeux.
    void Emit(RenderQueue& q, const Camera& cam) const override {
        const float s = scale, fx = m::Cos(heading), fy = m::Sin(heading), px = -fy, py = fx;
        const float zb = 6.f * s + m::Sin(phase_ * 2) * 0.5f * s;
        const Col leg = Shade(color, 0.82f), hi = Shade(color, 1.35f);

        // Corps : abdomen, petite taille (cache la jonction), grosse tête — profondeur = centre de la sphère
        const float hOff = 5.5f * s, hR = 8.5f * s, hZ = zb + s;
        const float off[3] = {-6.5f, -0.5f, 5.5f}, rad[3] = {7.5f, 4.2f, 8.5f}, zo[3] = {0.f, 0.5f, 1.f};
        for (int i = 0; i < 3; ++i) {
            float wx = x + fx * off[i] * s, wy = y + fy * off[i] * s, wz = zb + zo[i] * s;
            q.Sphere(cam.Depth(wx, wy, wz), wx, wy, wz, rad[i] * s, color);
        }
        const float hx = x + fx * hOff, hy = y + fy * hOff;
        const float headDepth = cam.Depth(0, hy, hZ);

        // Pattes courtes et épaisses, même couleur que le corps, attachées à la taille (3 paires)
        const int n = legPairs < 3 ? 3 : (legPairs > 4 ? 4 : legPairs);
        for (int side = -1; side <= 1; side += 2)
            for (int i = 0; i < n; ++i) {
                float u = i - (n - 1) * .5f, un = u / ((n - 1) * .5f);          // -1 arrière .. +1 avant
                float along = (u * 3.4f - 1.f) * s;
                float ph = phase_ + i * 2.09f + (side > 0 ? PI : 0);
                float stride = m::Sin(ph) * 2.2f * s, lf = m::Max(0.f, m::Cos(ph)) * 2.5f * s;
                float x0 = x + fx * along, y0 = y + fy * along;
                float reach = (11.5f - 1.5f * (un < 0 ? -un : un)) * s;
                float ex = x0 + px * side * reach + fx * (un * 4.f * s + stride);
                float ey = y0 + py * side * reach + fy * (un * 4.f * s + stride);
                float kx = x0 + px * side * reach * .55f + fx * (un * 1.5f * s + stride * .5f);
                float ky = y0 + py * side * reach * .55f + fy * (un * 1.5f * s + stride * .5f);
                Vec2 A = cam.Project(x0, y0, zb - 1.5f * s), K = cam.Project(kx, ky, zb + 1.5f * s + lf), F = cam.Project(ex, ey, lf);
                float dp = cam.Depth(0, (y0 + ky + ey) / 3.f, zb - 2.f * s);    // côté lointain derrière, proche devant
                q.Taper(dp, A, K, 3.4f * s, 2.8f * s, leg);
                q.Taper(dp, K, F, 2.8f * s, 1.2f * s, leg);
                q.Ellipse(dp, F, 1.1f * s, 1.1f * s * cam.tilt + .2f * s, Shade(color, 0.7f));
            }

        // Antennes = grosses "oreilles" effilées avec reflet, posées sur le dessus de la tête (toujours devant la tête)
        float sway = m::Sin(t_ * 5) * 0.8f * s;
        for (int side = -1; side <= 1; side += 2) {
            float bx = hx + px * side * 4.2f * s, by = hy + py * side * 4.2f * s, bz = hZ + 6.0f * s;
            float tx = bx + (fx * 2.0f + px * side * 5.0f) * s + px * side * sway, ty = by + (fy * 2.0f + py * side * 5.0f) * s + py * side * sway;
            Vec2 B = cam.Project(bx, by, bz), T = cam.Project(tx, ty, bz + 5.5f * s);
            float dp = headDepth + hR * 1.1f;
            q.Taper(dp, B, T, 3.6f * s, 2.0f * s, leg);
            q.Ellipse(dp, T, 1.0f * s, 1.0f * s, leg);
            q.Taper(dp + 0.01f, B, T, 1.5f * s, 0.8f * s, hi);              // reflet intérieur
        }

        // Yeux : grands, blancs, pupille vers l'avant, paupière sombre inclinée (regard déterminé), toujours visibles
        Vec2 hc = cam.Project(hx, hy, hZ);
        for (int side = -1; side <= 1; side += 2) {
            Vec2 e = cam.Project(hx + (fx * 4.4f + px * side * 4.6f) * s, hy + (fy * 4.4f + py * side * 4.6f) * s, hZ + 5.2f * s);
            Vec2 pu = cam.Project(hx + (fx * 5.6f + px * side * 4.6f) * s, hy + (fy * 5.6f + py * side * 4.6f) * s, hZ + 5.2f * s);
            float dir = hc.x > e.x ? 1.f : -1.f;                             // vers le centre de la tête (écran)
            float dp = headDepth + hR * 0.95f;
            q.Ellipse(dp, e, 2.7f * s, 2.9f * s, {255, 255, 255, 255});
            q.Ellipse(dp + 0.01f, {e.x + (pu.x - e.x) * .7f, e.y + (pu.y - e.y) * .7f + .3f * s}, 1.4f * s, 1.6f * s, {25, 15, 20, 255});
            q.Line(dp + 0.02f, {e.x + dir * 3.f * s, e.y - 1.9f * s}, {e.x - dir * 3.f * s, e.y - 3.0f * s}, 1.3f * s, {55, 15, 30, 255});
        }
    }

private:
    unsigned Rnd() { rng_ = rng_ * 1664525u + 1013904223u; return rng_ >> 16; }
    float tx_ = 0, ty_ = 0, phase_ = 0, t_ = 0, dropX_ = 0, dropY_ = 0, avX_ = 0, avY_ = 0, avT_ = 0;
    Vec2 wp_[40]; int nwp_ = 0, iwp_ = 0;
    float stuckAcc_ = 0, sx_ = 0, sy_ = 0, esc_ = 0, escSign_ = 1; int stuck_ = 0; unsigned rng_ = 1;
    bool  hasTarget_ = false; int state_ = 0;      // 0 libre, 1 va chercher, 2 porte
    Box*  box_ = nullptr;
};

// =============================================================================
//  SCENE : update, collisions, z-order global par pièce (tri fusion stable)
// =============================================================================
class Scene {
public:
    Camera camera;
    enum { MAX_CMDS = 16384 };
    Scene() { cmds_ = new DrawCmd[MAX_CMDS]; tmp_ = new DrawCmd[MAX_CMDS]; }
    ~Scene() { for (int i = 0; i < items_.Size(); ++i) delete items_[i]; delete[] cmds_; delete[] tmp_; }
    Scene(const Scene&) = delete; Scene& operator=(const Scene&) = delete;

    template <class T> T* Add() { T* e = new T(); items_.PushBack(e); return e; }

    void Update(float dt) {
        for (int i = 0; i < items_.Size(); ++i) items_[i]->Update(dt);
        Resolve();
        for (int i = 0; i < items_.Size(); ++i) items_[i]->LateUpdate();
        RemoveDead();
    }

    void Render(IPainter& p) {
        for (int i = 0; i < items_.Size(); ++i) items_[i]->DrawShadow(p, camera);
        RenderQueue q{cmds_, 0, MAX_CMDS};
        for (int i = 0; i < items_.Size(); ++i) items_[i]->Emit(q, camera);
        Sort(q.n);
        for (int i = 0; i < q.n; ++i) {
            const DrawCmd& c = cmds_[i];
            switch (c.kind) {
                case CMD_LINE:    p.DrawLine(c.p0, c.p1, c.a, c.col); break;
                case CMD_TAPER:   DrawTaper(p, c.p0, c.p1, c.a, c.b, c.col); break;
                case CMD_ELLIPSE: p.FillEllipse(c.p0, c.a, c.b, c.col); break;
                case CMD_SPHERE:  DrawSphere(p, camera, c.p0.x, c.p0.y, c.a, c.b, c.col); break;
                case CMD_CUSTOM:  static_cast<const ICustomDraw*>(c.ptr)->DrawPiece(p, camera, (int)c.a); break;
                case CMD_BOX:     static_cast<const Box*>(c.ptr)->DrawBody(p, camera); break;
            }
        }
    }

private:
    void RemoveDead() {
        int n = items_.Size(), dead = 0;
        for (int i = 0; i < n; ++i) if (items_[i]->dead) ++dead;
        if (!dead) return;
        Entity** live = new Entity*[n]; int k = 0;
        for (int i = 0; i < n; ++i) { if (items_[i]->dead) delete items_[i]; else live[k++] = items_[i]; }
        items_.Clear(); for (int i = 0; i < k; ++i) items_.PushBack(live[i]);
        delete[] live;
    }
    void Resolve() {
        int n = items_.Size();
        for (int i = 0; i < n; ++i) {
            Entity* a = items_[i]; if (a->kind != KIND_ANT) continue;
            Ant* A = static_cast<Ant*>(a);
            for (int j = 0; j < n; ++j) {                                     // obstacles génériques (plateau)
                Entity* o = items_[j];
                if (o->kind == KIND_BOARD) { float nx, ny; if (o->ResolveCircle(A->x, A->y, A->Radius(), nx, ny)) A->Nudge(nx, ny); }
            }
            for (int j = 0; j < n; ++j) {
                Entity* b = items_[j]; if (b == a || b->kind > KIND_BOX) continue;
                if (b->kind == KIND_ANT) {                                     // fourmi / fourmi (traité une fois)
                    if (j < i) continue;
                    Ant* B = static_cast<Ant*>(b);
                    float dx = B->x - A->x, dy = B->y - A->y, d = m::Hypot(dx, dy), md = A->Radius() + B->Radius();
                    if (d < md && d > 1e-4f) {
                        float nx = dx / d, ny = dy / d, pu = (md - d) * .5f;
                        A->x -= nx * pu; A->y -= ny * pu; B->x += nx * pu; B->y += ny * pu;
                        A->Nudge(-nx, -ny); B->Nudge(nx, ny);
                    }
                } else {                                                       // fourmi / boîte (non portée)
                    Box* B = static_cast<Box*>(b); if (B->carried) continue;
                    float cx, cy; B->Closest(A->x, A->y, cx, cy);
                    float dx = A->x - cx, dy = A->y - cy, d = m::Hypot(dx, dy), r = A->Radius();
                    if (d < r) {
                        float nx, ny;
                        if (d > 1e-4f) { nx = dx / d; ny = dy / d; }
                        else { float ex = A->x - B->x, ey = A->y - B->y, el = m::Max(1e-4f, m::Hypot(ex, ey)); nx = ex / el; ny = ey / el; }
                        A->x += nx * (r - d); A->y += ny * (r - d); A->Nudge(nx, ny);
                    }
                }
            }
        }
    }
    void Sort(int n) {                                                         // tri fusion ascendant, stable
        DrawCmd* src = cmds_; DrawCmd* dst = tmp_;
        for (int w = 1; w < n; w *= 2) {
            for (int lo = 0; lo < n; lo += 2 * w) {
                int mid = m::Min(lo + w, n), hi = m::Min(lo + 2 * w, n), i = lo, j = mid, k = lo;
                while (i < mid && j < hi) dst[k++] = (src[j].depth < src[i].depth) ? src[j++] : src[i++];
                while (i < mid) dst[k++] = src[i++];
                while (j < hi)  dst[k++] = src[j++];
            }
            DrawCmd* t = src; src = dst; dst = t;
        }
        if (src != cmds_) for (int i = 0; i < n; ++i) cmds_[i] = src[i];
    }
    Array<Entity*> items_;
    DrawCmd *cmds_, *tmp_;
};

} // namespace nk25d