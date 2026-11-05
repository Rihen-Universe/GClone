#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
"""embarquer_niveaux.py (version 2, 05/11/2026) : fabrique src/Nk25DGrillesData.h, les GRILLES de cubes des niveaux de FoodHunt,
embarquées dans l'exécutable à leur taille exacte (aucune lecture de fichier, aucun rééchantillonnage dans le jeu).

Pourquoi une nouvelle version (Rodolf : « les couleurs ou les formes ne ressemblent pas à des formes exactes et correctes ») :
l'ancienne source était le style « 3D » de Fluent Emoji, plein de dégradés ; la moyenne par case, puis la réduction des
couleurs, puis le rabattement sur la palette faisaient des taches et des bords flous (ancienne version : retire/). La chaîne :

  1. SOURCE : le style FLAT de Fluent Emoji (Resources/aliments_plats/*.svg, licence MIT), des aplats sans dégradé ;
  2. RASTÉRISATION : Chrome sans fenêtre dessine chaque SVG en 1024 x 1024 sur fond transparent ;
  3. GRILLE : la boîte englobante est centrée (les symétries sont gardées) ; chaque case échantillonne 6 x 6 pixels ;
     une case est PLEINE si l'image la couvre à plus de 50 % ; sa couleur est la MAJORITAIRE de ses pixels (jamais une
     moyenne : aucune couleur de transition n'existe) ;
  4. COULEURS PAR TEINTE : chaque pixel va vers une couleur de la palette de jeu (12) d'après sa TEINTE d'abord (rouge,
     orange, jaune, vert...), puis sa saturation et sa valeur ; jamais par simple distance RVB (un orange ne devient pas rose) ;
  5. SIMPLIFICATION : au plus 5 couleurs + le contour ; les couleurs rares passent à la plus proche en teinte ; les îlots
     de 1 ou 2 cases sont absorbés par leur voisin majoritaire (sauf le brun sombre : pépins, yeux, détails voulus) ;
  6. CONTOUR : une case brun sombre tout autour de la silhouette (voisinage à 4), pour une forme franche ;
  7. ÉCRITURE : tableau d'octets par niveau (255 = case vide, sinon l'indice dans la palette locale) + palette locale
     (indices dans la palette de jeu, la MÊME que Nk25DArt.h : GameColor).

Usage (depuis D:\\Live\\GClone) :  python FoodHunt/tools/embarquer_niveaux.py [--planche sortie.png]
Chrome est cherché dans les emplacements usuels de Windows ; les rasters sont gardés dans un dossier temporaire.
"""
import colorsys
import math
import os
import subprocess
import sys
import tempfile
from PIL import Image

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(RACINE, "Resources", "aliments_plats")
SORTIE = os.path.join(RACINE, "FoodHunt", "src", "Nk25DGrillesData.h")

# La palette de jeu : MÊME ordre que GameColor dans Nk25DArt.h. Indices : 0 rouge, 1 orange, 2 jaune, 3 vert, 4 turquoise,
# 5 bleu, 6 violet, 7 rose, 8 blanc, 9 brun sombre (contour), 10 crème, 11 brun clair.
PAL = [(228, 52, 52), (255, 146, 30), (250, 214, 40), (84, 190, 62), (28, 190, 180), (52, 112, 232),
       (146, 76, 204), (250, 122, 190), (245, 245, 240), (104, 66, 40), (252, 228, 176), (196, 138, 80)]
ROUGE, ORANGE, JAUNE, VERT, TURQ, BLEU, VIOLET, ROSE, BLANC, SOMBRE, CREME, CLAIR = range(12)
SUBS = 6            # sous-échantillons par côté de case
RASTER = 1024


def classer(r, g, b):
    """Couleur de la palette de jeu pour un pixel : par la TEINTE d'abord, puis saturation et valeur."""
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    h *= 360.0
    if v < 0.30:
        return SOMBRE
    if s < 0.14:
        return BLANC if v > 0.62 else SOMBRE
    if h >= 345 or h < 14:                      # rouges
        if v < 0.50:
            return SOMBRE
        return ROSE if (s < 0.52 and v > 0.80) else ROUGE
    if h < 40:                                  # oranges et bruns
        if v < 0.50:
            return SOMBRE
        if v < 0.80:
            return CLAIR
        return CREME if s < 0.48 else ORANGE
    if h < 68:                                  # jaunes
        if v < 0.55:
            return SOMBRE
        if s < 0.45:
            return CREME
        return CLAIR if v < 0.78 else JAUNE
    if h < 178:
        return VERT if not (s < 0.28 and v > 0.85) else BLANC
    if h < 200:
        return TURQ
    if h < 255:
        return BLEU
    if h < 300:
        return VIOLET
    return ROSE


def vec(i):
    """Position d'une couleur de palette dans un cône teinte/saturation/valeur : la teinte pèse d'abord."""
    h, s, v = colorsys.rgb_to_hsv(*[c / 255.0 for c in PAL[i]])
    return (math.cos(h * 6.2832) * s * 1.6, math.sin(h * 6.2832) * s * 1.6, v * 0.7)


def proche_teinte(i, gardees):
    a = vec(i)
    return min(gardees, key=lambda j: sum((x - y) ** 2 for x, y in zip(a, vec(j))))


def trouver_chrome():
    for p in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"):
        if os.path.exists(p):
            return p
    raise SystemExit("Chrome introuvable")


def rasteriser(svg, png, chrome, tmp):
    html = os.path.join(tmp, os.path.basename(svg) + ".html")
    with open(html, "w", encoding="utf-8") as f:
        f.write('<html><body style="margin:0;background:transparent"><img src="file:///%s" width="%d" height="%d" '
                'style="display:block"></body></html>' % (svg.replace("\\", "/"), RASTER, RASTER))
    subprocess.run([chrome, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--allow-file-access-from-files",
                    "--default-background-color=00000000", "--window-size=%d,%d" % (RASTER, RASTER),
                    "--screenshot=" + png, "file:///" + html.replace("\\", "/")],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)


def grille(png, largeur):
    """Image rastérisée -> (nx, ny, cases) ; cases[y][x] = indice de la palette de jeu, ou -1 (vide). Sans le contour."""
    im = Image.open(png).convert("RGBA")
    px = im.load()
    boite = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    bx0, by0, bx1, by1 = boite
    bw, bh = bx1 - bx0, by1 - by0
    interieur = largeur - 2                       # cases du plus grand côté, sans le contour
    cp = max(bw, bh) / float(interieur)           # pixels par case
    nx, ny = max(1, round(bw / cp)), max(1, round(bh / cp))
    ox = (bx0 + bx1) / 2.0 - nx * cp / 2.0        # boîte CENTRÉE : les symétries sont gardées
    oy = (by0 + by1) / 2.0 - ny * cp / 2.0
    cases = [[-1] * nx for _ in range(ny)]
    for cy in range(ny):
        for cx in range(nx):
            votes = {}
            pleins = 0
            for sy in range(SUBS):
                for sx in range(SUBS):
                    x = int(ox + (cx + (sx + .5) / SUBS) * cp)
                    y = int(oy + (cy + (sy + .5) / SUBS) * cp)
                    if 0 <= x < RASTER and 0 <= y < RASTER:
                        r, g, b, a = px[x, y]
                        if a >= 128:
                            pleins += 1
                            c = classer(r, g, b)
                            votes[c] = votes.get(c, 0) + 1
            if pleins * 2 > SUBS * SUBS:           # couverte à plus de 50 %
                cases[cy][cx] = max(votes, key=lambda k: (votes[k], -k))   # la MAJORITAIRE
    return nx, ny, cases


def limiter(cases, garder):
    cnt = {}
    for row in cases:
        for c in row:
            if c >= 0 and c != SOMBRE:
                cnt[c] = cnt.get(c, 0) + 1
    gardees = sorted(cnt, key=lambda k: -cnt[k])[:garder]
    for row in cases:
        for i, c in enumerate(row):
            if c >= 0 and c != SOMBRE and c not in gardees:
                row[i] = proche_teinte(c, gardees)


def absorber_ilots(cases, nx, ny, taille_max=2):
    """Les îlots (composantes à 4 voisins de même couleur) de 1 ou 2 cases passent à leur voisin majoritaire ; le brun sombre est gardé."""
    vu = [[False] * nx for _ in range(ny)]
    for y0 in range(ny):
        for x0 in range(nx):
            if vu[y0][x0] or cases[y0][x0] < 0:
                continue
            col = cases[y0][x0]
            comp, pile = [], [(x0, y0)]
            vu[y0][x0] = True
            while pile:
                x, y = pile.pop()
                comp.append((x, y))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    a, b = x + dx, y + dy
                    if 0 <= a < nx and 0 <= b < ny and not vu[b][a] and cases[b][a] == col:
                        vu[b][a] = True
                        pile.append((a, b))
            if len(comp) <= taille_max and col != SOMBRE:
                voisins = {}
                for x, y in comp:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        a, b = x + dx, y + dy
                        if 0 <= a < nx and 0 <= b < ny and cases[b][a] >= 0 and cases[b][a] != col:
                            voisins[cases[b][a]] = voisins.get(cases[b][a], 0) + 1
                if voisins:
                    nouv = max(voisins, key=lambda k: voisins[k])
                    for x, y in comp:
                        cases[y][x] = nouv


def contour(cases, nx, ny):
    """Ajoute une case SOMBRE tout autour de la silhouette (voisinage à 4) : la grille gagne une case de marge de chaque côté."""
    w, h = nx + 2, ny + 2
    out = [[-1] * w for _ in range(h)]
    for y in range(ny):
        for x in range(nx):
            out[y + 1][x + 1] = cases[y][x]
    for y in range(h):
        for x in range(w):
            if out[y][x] >= 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = x + dx, y + dy
                if 1 <= a <= nx and 1 <= b <= ny and cases[b - 1][a - 1] >= 0:
                    out[y][x] = SOMBRE
                    break
    return w, h, out


def niveau(png, largeur, garder):
    nx, ny, cases = grille(png, largeur)
    limiter(cases, garder)
    absorber_ilots(cases, nx, ny)
    return contour(cases, nx, ny)


def main():
    planche = None
    if "--planche" in sys.argv:
        planche = sys.argv[sys.argv.index("--planche") + 1]
    chrome = trouver_chrome()
    tmp = tempfile.mkdtemp(prefix="foodhunt_")
    exclus = {"pineapple", "pear"}   # restent dans Resources/aliments_plats : ananas illisible, poire trop terne (un seul vert) ; remplacés par le champignon
    noms = sorted(f[:-4] for f in os.listdir(SRC) if f.endswith(".svg") and f[:-4] not in exclus)
    pre = []                                             # première passe : difficulté (aire + variété), grille de 40
    for nom in noms:
        png = os.path.join(tmp, nom + ".png")
        rasteriser(os.path.join(SRC, nom + ".svg"), png, chrome, tmp)
        w, h, g = niveau(png, 40, 5)
        cnt = {}
        for row in g:
            for c in row:
                if c >= 0:
                    cnt[c] = cnt.get(c, 0) + 1
        pre.append((sum(cnt.values()) + 40 * len(cnt), nom, png))
    pre.sort()
    # Les premiers niveaux attirent : images TRÈS colorées et simples en tête (Rodolf, 05/11 : « un niveau 1 presque tout blanc n'attire pas »).
    tete = ["strawberry", "cherries", "grapes", "carrot", "watermelon", "mushroom", "pizza", "taco"]
    pre = [p for t in tete for p in pre if p[1] == t] + [p for p in pre if p[1] not in tete]
    niveaux = []
    for rang, (_, nom, png) in enumerate(pre):
        largeur = 32 + (rang * 12) // 24                 # 32 pour les tout premiers, 44 pour les derniers
        garder = 4 if rang < 8 else 5                    # + le contour : 5 ou 6 couleurs au plus
        w, h, g = niveau(png, largeur, garder)
        usage = sorted({c for row in g for c in row if c >= 0})
        local = {c: i for i, c in enumerate(usage)}
        octets = bytes(255 if c < 0 else local[c] for row in g for c in row)
        niveaux.append((nom, w, h, usage, octets))
    sortie = ["// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen",
              "// Nk25DGrillesData.h — GÉNÉRÉ par tools/embarquer_niveaux.py : ne pas modifier à la main.",
              "// %d grilles de cubes (Fluent Emoji, style Flat, licence MIT : Resources/aliments_plats/LICENCES.md), triées du plus"
              % len(niveaux), "// simple au plus dur, à leur taille exacte. 255 = case vide ; sinon indice dans la palette locale (indices de GameColor).",
              "#pragma once", "", "namespace nk25d {", ""]
    for i, (nom, w, h, usage, octets) in enumerate(niveaux):
        sortie.append("static const unsigned char kGrille%02d[] = {  // %s, %d x %d" % (i, nom, w, h))
        for j in range(0, len(octets), 40):
            sortie.append("    " + ",".join(str(b) for b in octets[j:j + 40]) + ",")
        sortie.append("};")
        sortie.append("static const unsigned char kPalette%02d[] = {%s};" % (i, ",".join(str(c) for c in usage)))
    sortie += ["", "struct GrilleEmbarquee { int largeur, hauteur; const unsigned char* cases; int nbCouleurs; const unsigned char* couleurs; };",
               "static const GrilleEmbarquee kGrillesEmbarquees[] = {"]
    for i, (nom, w, h, usage, octets) in enumerate(niveaux):
        sortie.append("    { %d, %d, kGrille%02d, %d, kPalette%02d },  // %s" % (w, h, i, len(usage), i, nom))
    sortie += ["};", "static const int kNbGrillesEmbarquees = %d;" % len(niveaux), "", "} // namespace nk25d", ""]
    with open(SORTIE, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(sortie))
    print("%d grilles ecrites : %s" % (len(niveaux), SORTIE))
    for i, (nom, w, h, usage, octets) in enumerate(niveaux):
        print("%2d %-16s %2d x %2d  couleurs %s" % (i + 1, nom, w, h, usage))
    if planche:
        cote, cols = 9, 5
        cw = max(n[1] for n in niveaux) * cote
        ch = max(n[2] for n in niveaux) * cote
        P = Image.new("RGB", (cols * (cw + 20), 5 * (ch + 20)), (176, 218, 118))
        for i, (nom, w, h, usage, octets) in enumerate(niveaux):
            tile = Image.new("RGB", (w * cote, h * cote), (244, 239, 229))
            for y in range(h):
                for x in range(w):
                    v = octets[y * w + x]
                    if v != 255:
                        col = PAL[usage[v]]
                        for yy in range(cote - 1):
                            for xx in range(cote - 1):
                                tile.putpixel((x * cote + xx, y * cote + yy), col)
            P.paste(tile, ((i % cols) * (cw + 20) + 10 + (cw - w * cote) // 2, (i // cols) * (ch + 20) + 10 + (ch - h * cote) // 2))
        P.save(planche)
        print("planche : " + planche)


if __name__ == "__main__":
    sys.exit(main())
