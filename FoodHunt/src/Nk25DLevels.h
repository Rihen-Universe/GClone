// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nk25DLevels.h — Chargement d'images et de SOUS-IMAGES pour fabriquer des niveaux.
// Migré le 04/11/2026 : le décodage est NkImage::Load (NKImage), les rectangles d'une planche sont ceux de
// renderer::NkSpriteSheet (NKCanvas), le fond et le détourage viennent de NkImagePalette. Ne reste ici que ce
// qui est propre au jeu : la classe SpriteSheet (une image + ses rectangles + l'extraction détourée) et la
// suite ordonnée de niveaux LevelLibrary.
//
// Chaîne de traitement (de la planche d'icônes jusqu'aux blocs du plateau) :
//
//   fichier ──NkImage::Load──────► NkImage (RGBA)           [décodage]
//           ──NkSpriteSheet──────► liste de rectangles       [grille manuelle OU détection auto]
//           ──Extract────────────► sous-image RGBA détourée  [NkImage::Crop + RemoveBackground]
//           ──Board::LoadFromImage► grille de blocs          [réduction NK_AREA + k-means]
//
// "N'importe quelle image ?"  Oui : tout ce que NkImage décode (PNG, JPEG, BMP...).
//   - PNG avec transparence : la transparence est utilisée telle quelle.
//   - JPEG / image opaque   : le fond (estimé sur les bords) est retiré automatiquement.
//   - Le résultat est le meilleur avec des formes nettes sur fond clair uni (icônes, logos,
//     pixel-art). Une photo marche, mais la palette réduite (6-8 couleurs) la "postérise".
// =============================================================================
#pragma once
#include "Nk25DBoard.h"
#include "NKCanvas/Renderer/Resources/NkSpriteSheet.h"

namespace nk25d {

// =============================================================================
//  SpriteSheet : une grande image + une liste de rectangles (les "sous-images").
// =============================================================================
class SpriteSheet {
public:
    bool Load(const char* path) {
        if (!img_.Load(path, 4)) return false;
        bg_ = NkImagePalette::EstimateBg(img_); return true;
    }
    bool SetImage(const NkImage& im) {                                  // copie depuis une image existante
        img_ = im.Copy(); if (!img_.IsValid()) return false;
        bg_ = NkImagePalette::EstimateBg(img_); return true;
    }
    int  Count() const { return sheet_.Count(); }
    const NkRect2i& Rect(int i) const { return sheet_.Get(i)->rect; }
    int  Width() const { return img_.Width(); }
    int  Height() const { return img_.Height(); }

    // ---- Découpe MANUELLE : grille régulière --------------------------------------------------
    // cols x rows cellules dans la zone (image moins marges en pixels). `ins*` = fractions (0..1) de
    // CHAQUE cellule ignorées sur le bord : utile pour exclure une légende ou une case à cocher
    // (ex. planche 2 : le texte est sous l'icône => insB = 0.3). C'est la « zone utile » de NkSpriteSheet.
    // Les rectangles sont numérotés dans l'ordre de lecture (gauche->droite, haut->bas).
    int SetGrid(int cols, int rows, int mL = 0, int mT = 0, int mR = 0, int mB = 0,
                float insL = 0, float insT = 0, float insR = 0, float insB = 0) {
        renderer::NkSheetGrid g;
        g.colonnes = cols; g.lignes = rows; g.margeGauche = mL; g.margeHaut = mT; g.margeDroite = mR; g.margeBas = mB;
        g.utileX = insL; g.utileY = insT; g.utileL = 1.f - insL - insR; g.utileH = 1.f - insT - insB;
        return sheet_.BuildGrid(img_.Width(), img_.Height(), g);
    }

    // ---- Découpe AUTOMATIQUE : profils de projection ------------------------------------------
    // Idée (segmentation par histogrammes de projection, classique en analyse de documents) : les lignes
    // sans pixel « non-fond » forment des GOUTTIÈRES horizontales, et entre deux gouttières se trouve une bande
    // d'icônes ; dans chaque bande, les colonnes vides séparent les icônes. Voir NkSpriteSheet::BuildAuto.
    int AutoDetect(int minSize = 12, int bgTol = 55, int minGap = 3) { return sheet_.BuildAuto(img_, bg_, bgTol, minGap, minSize); }

    void Rename(int i, const char* nom) { sheet_.Rename(i, nom); }

    // ---- Extraction d'une sous-image détourée ---------------------------------------------------
    // 1. copie du rectangle (NkImage::Crop) ; 2. FOND = pixels proches de la couleur de fond ET reliés au bord
    //    par un chemin de pixels de fond (RemoveBackground, remplissage 4-connexe) -> alpha 0. Les blancs
    //    INTÉRIEURS à l'icône (blanc d'oeuf) ne sont pas reliés au bord, donc ils restent opaques ;
    // 3. recadrage serré sur la boîte englobante du contenu (+1 px).
    bool Extract(int idx, NkImage& out, int bgTol = 55) const {
        if (idx < 0 || idx >= Count()) return false;
        NkRect2i r = Rect(idx);
        r.left = NkClamp(r.left, 0, img_.Width() - 1); r.top = NkClamp(r.top, 0, img_.Height() - 1);
        r.width = NkMin(r.width, img_.Width() - r.left); r.height = NkMin(r.height, img_.Height() - r.top);
        if (r.width < 2 || r.height < 2) return false;
        NkImage sub = img_.Crop(r.left, r.top, r.width, r.height);
        if (!sub.IsValid()) return false;
        NkImagePalette::RemoveBackground(sub, bg_, bgTol);
        int x0 = sub.Width(), y0 = sub.Height(), x1 = -1, y1 = -1;       // boîte englobante du contenu
        for (int y = 0; y < sub.Height(); ++y) for (int x = 0; x < sub.Width(); ++x)
            if (sub.RowPtr(y)[x * 4 + 3] >= 128) { x0 = NkMin(x0, x); y0 = NkMin(y0, y); x1 = NkMax(x1, x); y1 = NkMax(y1, y); }
        if (x1 < x0 || y1 < y0) return false;
        x0 = NkMax(0, x0 - 1); y0 = NkMax(0, y0 - 1); x1 = NkMin(sub.Width() - 1, x1 + 1); y1 = NkMin(sub.Height() - 1, y1 + 1);
        out = sub.Crop(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        return out.IsValid();
    }

private:
    NkImage img_; renderer::NkSpriteSheet sheet_; NkColor bg_{255, 255, 255, 255};
};

// =============================================================================
//  LevelLibrary : suite ordonnée de niveaux (chaque niveau = une sous-image d'une planche).
//  "Charger l'image suivante" = LoadLevel(i + 1).
// =============================================================================
class LevelLibrary {
public:
    SpriteSheet* NewSheet() { sheets_.PushBack(NkMakeUnique<SpriteSheet>()); return sheets_[sheets_.Size() - 1].Get(); }   // la bibliothèque possède la planche
    int  AddAll(SpriteSheet* s) { for (int i = 0; i < s->Count(); ++i) refs_.PushBack(Ref{s, i}); return Count(); }   // un niveau par rectangle
    int  AddOne(SpriteSheet* s, int idx) { if (idx >= 0 && idx < s->Count()) refs_.PushBack(Ref{s, idx}); return Count(); }
    int  Count() const { return (int)refs_.Size(); }

    // Charge le niveau i dans `b`. `blocksAcross` = nombre de blocs sur la plus grande dimension de l'image
    // (40 => ~800 à 1500 blocs) ; `maxColors` = taille de la palette (nombre de couleurs de fourmis).
    bool LoadLevel(int i, Board& b, int blocksAcross = 40, int maxColors = 6) const {
        if (i < 0 || i >= Count()) return false;
        NkImage img; if (!refs_[i].s->Extract(refs_[i].idx, img)) return false;
        // Conserver le rapport d'aspect : la plus grande dimension reçoit `blocksAcross` blocs.
        int gridW = img.Width() >= img.Height() ? blocksAcross : NkMax(6, (int)(blocksAcross * (float)img.Width() / img.Height() + .5f));
        gridW = NkMin(gridW, img.Width());                              // jamais plus de blocs que de pixels
        return b.LoadFromImage(img, gridW, maxColors, false);           // fond déjà retiré (alpha)
    }
private:
    struct Ref { SpriteSheet* s; int idx; };
    NkVector<Ref> refs_; NkVector<NkUniquePtr<SpriteSheet>> sheets_;
};

} // namespace nk25d
