# FoodHunt

Jeu de plateau de cubes 2.5D : des fourmis sortent des paniers, prennent les cubes de leur couleur et les rapportent au nid.
Habillage « pique-nique au jardin » dessiné par le code ; 25 niveaux (images Fluent Emoji, MIT) embarqués dans l'exécutable.

- Construire : `python -m Jenga build --target FoodHunt --config Release --platform Windows-x86_64 --no-daemon` (depuis GClone).
- Niveaux : `python FoodHunt/tools/embarquer_niveaux.py` régénère `src/Nk25DGrillesData.h`.
- Banc : `FoodHunt.exe --bot --verifier-niveaux` (sans fenêtre ; code 0 = les 25 niveaux passent). Contre-épreuves :
  `--casser-niveau=3 --niveau=3` et `--ancien-comportement --niveau=3` doivent donner un code non nul.
- Options d'essai : `--bot`, `--niveau=N`, `--avance=S`, `--fenetre=LxH`, `--capture=... --capture-frame=N`.
- Vue : `src/NkView25D.h` (hérite de `NkView2D`, sans dépendance au jeu).
