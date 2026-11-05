# GClone

Des jeux construits avec **Nkentseu**, le moteur C++ de Rihen Universe, à commencer par **FoodHunt** : un jeu en
2.5D où des fourmis cherchent leur nourriture sur un plateau.

Ce dépôt sert de support à des lives : on y voit un jeu écrit d'abord avec ses propres outils, puis ramené
sur les briques de Nkentseu (mathématiques, images, dessin, recherche de chemin), sans rien réimplémenter.

## Ce qu'il faut

- **NKCode 1.5.6** ou plus (son Jenga embarqué), ou **Jenga 2.9** ou plus.
- **Le kit NKCanvas de Nkentseu**, à décompresser dans `Kits/` (on obtient `Kits/NKCanvas/`). Il est publié
  dans [Nkentseu-Kits](https://github.com/Rihen-Universe/Nkentseu-Kits/releases/tag/kits-canvas-2026-11-04), une
  archive par plateforme (sous Windows : `NKCanvas-kit-Windows-x86_64.zip`).
  Le kit ne va pas dans ce dépôt : ce sont des bibliothèques précompilées.
  **Attention** : FoodHunt utilise des briques ajoutées à Nkentseu le 04/11/2026 (`NkSpriteSheet`,
  `NkGridPath`, `NkImagePalette`…). Il lui faut donc ce kit ou un plus récent ; le kit du 22/10 ne suffit pas
  (celui-là convient au premier commit de ce dépôt).

## Construire et lancer

Depuis la racine du dépôt :

```
jenga build --target FoodHunt --config Debug
Build\Bin\Debug-Windows\FoodHunt\FoodHunt.exe
```

Dans NKCode : ouvrir le dossier `GClone`, puis Ctrl+B pour construire et Ctrl+R pour lancer. Le programme
démarre dans la racine du dossier ouvert.

## Images

Les aliments de `Resources/aliments_libres/` viennent de Fluent Emoji de Microsoft (licence MIT) : voir
`Resources/aliments_libres/LICENCES.md`.

## Licence

Voir [LICENSE](LICENSE) : licence propriétaire de Rihen Universe (Nkentseu, Jenga et les jeux).
