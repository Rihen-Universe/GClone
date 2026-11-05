# Aliments libres de droits (images pour la pixelisation 2.5D de FoodHunt)

Vingt-cinq images PNG en couleur (256 x 256, fond transparent), prises dans **Fluent Emoji de Microsoft**.

- Source : https://github.com/microsoft/fluentui-emoji (dossier `assets/<Nom>/3D/`, style « 3D », PNG)
- Licence : **MIT** (« MIT license » indiquée sur la page du dépôt, `LICENSE` : « Copyright (c) Microsoft Corporation. »)
- URL de la licence : https://github.com/microsoft/fluentui-emoji/blob/main/LICENSE
- Vérifiée sur la page du dépôt et dans le fichier `LICENSE` le 04/11/2026.
- Les fichiers sont copiés tels quels, seul le nom est simplifié (`hamburger_3d.png` devient `hamburger.png`).

## Texte de la licence MIT (reproduit, comme l'exige la licence)

MIT License

Copyright (c) Microsoft Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of
the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Liste des images

| Fichier | Emoji | Dossier source |
|---|---|---|
| hamburger.png | Hamburger | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Hamburger/3D |
| pizza.png | Pizza | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Pizza/3D |
| french_fries.png | French fries | https://github.com/microsoft/fluentui-emoji/tree/main/assets/French%20fries/3D |
| hot_dog.png | Hot dog | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Hot%20dog/3D |
| sandwich.png | Sandwich | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Sandwich/3D |
| cheese_wedge.png | Cheese wedge | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Cheese%20wedge/3D |
| croissant.png | Croissant | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Croissant/3D |
| baguette_bread.png | Baguette bread | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Baguette%20bread/3D |
| pancakes.png | Pancakes | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Pancakes/3D |
| doughnut.png | Doughnut | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Doughnut/3D |
| cookie.png | Cookie | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Cookie/3D |
| shortcake.png | Shortcake | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Shortcake/3D |
| soft_ice_cream.png | Soft ice cream | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Soft%20ice%20cream/3D |
| red_apple.png | Red apple | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Red%20apple/3D |
| banana.png | Banana | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Banana/3D |
| strawberry.png | Strawberry | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Strawberry/3D |
| watermelon.png | Watermelon | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Watermelon/3D |
| carrot.png | Carrot | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Carrot/3D |
| avocado.png | Avocado | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Avocado/3D |
| cherries.png | Cherries | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Cherries/3D |
| pineapple.png | Pineapple | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Pineapple/3D |
| grapes.png | Grapes | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Grapes/3D |
| popcorn.png | Popcorn | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Popcorn/3D |
| cupcake.png | Cupcake | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Cupcake/3D |
| taco.png | Taco | https://github.com/microsoft/fluentui-emoji/tree/main/assets/Taco/3D |

Les planches de Rodolf (« Food Scavenger Hunt », Envato) ne sont PAS dans ce dossier : usage personnel, lues depuis
son propre dossier seulement, jamais copiées ici.

## Embarquement dans l'exécutable (05/11/2026)

`FoodHunt/tools/embarquer_niveaux.py` recadre ces 25 images, les réduit à 96 px et les range, triées du plus simple au plus
dur, dans `FoodHunt/src/Nk25DNiveauxData.h` : le jeu ne lit plus aucun fichier à l'exécution. Les images dérivées restent
sous la même licence MIT (Microsoft Corporation) reproduite ci-dessus.
