// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nk25DLevels.h — Les niveaux de FoodHunt : vingt-cinq GRILLES de cubes EMBARQUÉES, triées du plus simple au plus dur.
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

class LevelLibrary {
public:
    int  LoadEmbedded() { return kNbGrillesEmbarquees; }        // rien à décoder : les grilles sont déjà des tableaux
    int  Count() const { return kNbGrillesEmbarquees; }

    // Charge le niveau i dans `b`, tel quel.
    bool LoadLevel(int i, Board& b) const {
        if (i < 0 || i >= Count()) return false;
        const GrilleEmbarquee& g = kGrillesEmbarquees[i];
        return b.LoadFromGrid(g.largeur, g.hauteur, g.cases, g.nbCouleurs, g.couleurs);
    }
};

} // namespace nk25d
