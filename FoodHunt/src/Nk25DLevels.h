// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nk25DLevels.h — Les niveaux de FoodHunt : vingt-cinq images EMBARQUÉES, triées du plus simple au plus dur.
// Réécrit le 05/11/2026. Avant, ce fichier portait une classe locale `SpriteSheet` (planche d'icônes + rectangles +
// extraction détourée) : les 25 niveaux sont maintenant des images SÉPARÉES, déjà recadrées et détourées par
// tools/embarquer_niveaux.py, donc la classe n'a plus de raison d'être dans le chemin du jeu. Elle est déplacée,
// non supprimée, dans retire/Nk25DLevelsAvecSpriteSheet.h (le moteur a sa propre planche : renderer::NkSpriteSheet).
//
// Chaîne de traitement :
//   tableau C compilé ──NkImage::LoadFromMemory──► NkImage RGBA (fond transparent)   [décodage, aucune lecture de fichier]
//                     ──Board::LoadFromImage────► grille de blocs                      [réduction NK_AREA + k-means]
// Sur Windows, Linux, macOS, iOS, Android, HarmonyOS et Web le jeu se comporte donc pareil : il n'ouvre aucun fichier.
// =============================================================================
#pragma once
#include "Nk25DBoard.h"
#include "Nk25DNiveauxData.h"

namespace nk25d {

class LevelLibrary {
public:
    // Décode toutes les images embarquées (une seule fois, au démarrage). Rend le nombre de niveaux chargés.
    int  LoadEmbedded() {
        imgs_.Clear();
        for (int i = 0; i < kNbNiveauxEmbarques; ++i) {
            NkImage im;
            if (im.LoadFromMemory(kNiveauxEmbarques[i].octets, (usize)kNiveauxEmbarques[i].taille, 4)) imgs_.PushBack(static_cast<NkImage&&>(im));
        }
        return Count();
    }
    int  Count() const { return (int)imgs_.Size(); }

    // Difficulté croissante : plus de blocs en largeur (26 -> 44) et plus de couleurs de fourmis (4 -> 8) à mesure
    // qu'on avance. Le tri des images (aire puis variété de couleurs) a été fait par l'outil ; ici on ne fait que
    // traduire le rang du niveau en finesse de la grille et en taille de palette.
    static void Params(int i, int& blocksAcross, int& maxColors) {
        blocksAcross = 26 + (i * 18) / 24;
        maxColors = i < 4 ? 4 : (i < 10 ? 5 : (i < 16 ? 6 : (i < 21 ? 7 : 8)));
    }

    // Charge le niveau i dans `b`. La plus grande dimension de l'image reçoit `blocksAcross` blocs (rapport d'aspect gardé).
    bool LoadLevel(int i, Board& b) const {
        if (i < 0 || i >= Count()) return false;
        int blocks, colors; Params(i, blocks, colors);
        const NkImage& img = imgs_[i];
        int gridW = img.Width() >= img.Height() ? blocks : NkMax(6, (int)(blocks * (float)img.Width() / img.Height() + .5f));
        gridW = NkMin(gridW, (int)img.Width());                          // jamais plus de blocs que de pixels
        return b.LoadFromImage(img, gridW, colors, false);               // fond déjà transparent
    }
private:
    NkVector<NkImage> imgs_;
};

} // namespace nk25d
