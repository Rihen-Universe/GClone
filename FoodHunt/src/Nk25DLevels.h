// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nk25DLevels.h : Les niveaux de FoodHunt : vingt-cinq GRILLES de cubes EMBARQUÉES, triées du plus simple au plus dur.
// Réécrit le 05/11/2026 (version « images nettes »). Avant, le jeu embarquait des PNG (style 3D de Fluent Emoji), les
// décodait (NkImage), les réduisait par moyenne de case et les quantifiait : des taches et des bords flous. Maintenant
// tools/embarquer_niveaux.py fait tout le travail à la construction (SVG « Flat » -> grille nette, couleur majoritaire par case,
// palette par teinte, contour sombre) et le jeu ne fait que COPIER la grille : aucun décodage, aucun rééchantillonnage,
// aucune quantification, aucune lecture de fichier. Windows, Linux, macOS, iOS, Android, HarmonyOS et Web se comportent pareil.
// L'ancienne version (PNG + SpriteSheet) est dans retire/.
// =============================================================================
#pragma once
#include "Nk25DBoard.h"
#include "Nk25DGrillesData.h"

namespace nk25d {

// -----------------------------------------------------------------------------
// LevelLibrary : la bibliothèque des niveaux. Elle ne contient aucune donnée elle-même : les grilles vivent dans
// Nk25DGrillesData.h (tableaux constants générés), et cette classe est la petite porte par laquelle le jeu les lit.
// Ce fichier s'articule avec les autres ainsi : Nk25DGrillesData.h fournit les tableaux, Nk25DBoard.h fournit le Board
// qui les reçoit (LoadFromGrid), et main.cpp appelle LoadLevel à chaque changement de niveau.
// -----------------------------------------------------------------------------
class LevelLibrary {
public:
    // Prépare la bibliothèque. Rend le nombre de niveaux disponibles.
    // Rien à décoder ni à lire sur le disque : les grilles sont déjà des tableaux dans l'exécutable, donc cet appel
    // ne fait que renvoyer le compte (il existe pour garder le même geste qu'avec l'ancienne version à images).
    int LoadEmbedded()
    {
        return kNbGrillesEmbarquees;
    }

    // Rend le nombre de niveaux embarqués (25 dans la version actuelle).
    int Count() const
    {
        return kNbGrillesEmbarquees;
    }

    // Charge le niveau d'indice i (0 = le premier, le plus simple) dans le plateau `b`, tel quel.
    //   i : indice du niveau, de 0 à Count() - 1.
    //   b : le plateau à remplir (il est écrasé par la grille du niveau).
    // Rend false si l'indice est hors des bornes ou si le plateau refuse la grille ; true si le niveau est chargé.
    bool LoadLevel(int i, Board& b) const
    {
        // Un indice négatif ou trop grand n'a pas de grille : on refuse sans toucher au plateau.
        if (i < 0 || i >= Count())
        {
            return false;
        }
        // La grille embarquée : largeur, hauteur, une valeur par case, et la palette de ce niveau.
        const GrilleEmbarquee& g = kGrillesEmbarquees[i];
        // Le plateau recopie la grille (aucun rééchantillonnage ni quantification : la grille est déjà finale).
        return b.LoadFromGrid(g.largeur, g.hauteur, g.cases, g.nbCouleurs, g.couleurs);
    }
};

} // namespace nk25d
