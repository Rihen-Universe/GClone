#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
"""embarquer_niveaux.py : fabrique src/Nk25DNiveauxData.h, les images des niveaux de FoodHunt DANS l'exécutable.

Pourquoi un en-tête : Windows, Linux, macOS, iOS, Android, HarmonyOS et Web doivent marcher pareil, donc aucune
lecture de fichier à l'exécution (le Web n'a pas de disque, Android range ses fichiers dans l'APK...). Les octets
des PNG deviennent des tableaux C compilés avec le jeu ; le jeu les décode par NkImage::LoadFromMemory.

Ce que fait l'outil, dans l'ordre :
  1. lit les PNG de Resources/aliments_libres (256 x 256, fond transparent) ;
  2. recadre sur la boîte englobante de l'alpha et réduit à 96 pixels au plus grand côté : le plateau n'a jamais
     plus de ~48 blocs en largeur, 96 pixels suffisent largement et les tableaux restent légers ;
  3. note la difficulté de chaque image : nombre de blocs du plateau (aire) + variété des couleurs + contour ;
  4. trie du plus simple au plus dur et écrit l'en-tête (tableau d'octets + table {octets, taille, blocs, couleurs}).

Usage (depuis D:\\Live\\GClone) :  python FoodHunt/tools/embarquer_niveaux.py
Les licences des images sont dans Resources/aliments_libres/LICENCES.md (Fluent Emoji, MIT).
"""
import io
import os
import sys
from PIL import Image

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(RACINE, "Resources", "aliments_libres")
SORTIE = os.path.join(RACINE, "FoodHunt", "src", "Nk25DNiveauxData.h")
COTE = 96


def preparer(chemin):
    im = Image.open(chemin).convert("RGBA")
    boite = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    im = im.crop(boite)
    k = COTE / max(im.size)
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    return im


def difficulte(im):
    """Aire (en cases d'une grille de 32), variété de couleurs (teintes à 4 bits, >2 % de l'aire) et contour."""
    g = im.resize((32, round(32 * im.height / im.width)) if im.width >= im.height
                  else (round(32 * im.width / im.height), 32), Image.BOX)
    px = list(g.getdata())
    plein = [p for p in px if p[3] >= 128]
    aire = len(plein)
    hist = {}
    for r, v, b, _ in plein:
        c = (r >> 6, v >> 6, b >> 6)
        hist[c] = hist.get(c, 0) + 1
    couleurs = sum(1 for n in hist.values() if n > 0.02 * max(1, aire))
    return aire + 25 * couleurs, aire, couleurs


def main():
    fichiers = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".png"))
    niveaux = []
    for f in fichiers:
        im = preparer(os.path.join(SRC, f))
        score, aire, coul = difficulte(im)
        tampon = io.BytesIO()
        im.save(tampon, "PNG", optimize=True)
        niveaux.append((score, f[:-4], tampon.getvalue(), aire, coul))
    niveaux.sort(key=lambda n: (n[0], n[1]))

    sortie = ["// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen",
              "// Nk25DNiveauxData.h — GÉNÉRÉ par tools/embarquer_niveaux.py : ne pas modifier à la main.",
              "// Les %d images des niveaux (Fluent Emoji de Microsoft, licence MIT : Resources/aliments_libres/LICENCES.md),"
              % len(niveaux),
              "// recadrées, réduites à %d px et triées du plus simple au plus dur." % COTE,
              "#pragma once", "", "namespace nk25d {", ""]
    for i, (_, nom, donnees, _, _) in enumerate(niveaux):
        sortie.append("static const unsigned char kNiveau%02d[] = {  // %s, %d octets" % (i, nom, len(donnees)))
        for j in range(0, len(donnees), 24):
            sortie.append("    " + ",".join(str(b) for b in donnees[j:j + 24]) + ",")
        sortie.append("};")
    sortie += ["", "struct NiveauEmbarque { const unsigned char* octets; unsigned int taille; };",
               "static const NiveauEmbarque kNiveauxEmbarques[] = {"]
    for i, (_, nom, donnees, aire, coul) in enumerate(niveaux):
        sortie.append("    { kNiveau%02d, %d },  // %s (aire %d, couleurs %d)" % (i, len(donnees), nom, aire, coul))
    sortie += ["};", "static const int kNbNiveauxEmbarques = %d;" % len(niveaux), "", "} // namespace nk25d", ""]
    with open(SORTIE, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(sortie))
    total = sum(len(n[2]) for n in niveaux)
    print("%d niveaux, %d octets de PNG, écrit : %s" % (len(niveaux), total, SORTIE))
    for i, n in enumerate(niveaux):
        print("%2d %-16s aire %4d couleurs %d" % (i + 1, n[1], n[3], n[4]))


if __name__ == "__main__":
    sys.exit(main())
