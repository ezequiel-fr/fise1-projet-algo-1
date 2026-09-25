# Pac-Man (arcade) dans le terminal

Version complète et indépendante du projet d'algorithmique : on y joue au
clavier (ou on regarde l'IA jouer), et les quatre fantômes suivent les règles
de la borne d'arcade de 1980. Les niveaux sont les cartes ASCII de la version
initiale (`../levels/levelN.map`), affichées avec les mêmes caractères.

## Lancer

Linux, WSL ou macOS. Le panneau d'informations s'affiche à droite du
labyrinthe si le terminal est assez large (environ 120 colonnes pour la carte
la plus large), sinon en dessous.

```sh
make          # compile ./pacman
make run      # jouer
make ia       # regarder l'IA jouer
make test     # vérifications automatiques, puis 10 parties jouées par l'IA
```

| Option | Rôle |
|---|---|
| `--carte FICHIER` | carte à jouer ; répétable (une carte par niveau, en boucle). `--carte arcade` : plan de la borne. Par défaut : `../levels/level1.map`, `level2.map`, `level3.map` |
| `--style ascii\|blocs` | `ascii` (défaut) : caractères de la version initiale ; `blocs` : murs pleins, deux colonnes par case |
| `--ia` | l'IA joue dès le départ |
| `--niveau N`, `--vitesse F`, `--graine N` | niveau de départ, facteur de vitesse (1 = arcade), graine du hasard |
| `--test N [--verbeux]` | vérifications, puis N parties jouées par l'IA sans affichage |

Caractères (ceux de la version initiale) : `*` mur, `.` point, `O` énergie,
`-` porte de la maison, `@` Pac-Man (`_` quand il est attrapé), `$` Blinky,
`#` Pinky, `%` Inky, `&` Clyde (en bleu quand ils sont effrayés), `"` yeux d'un
fantôme mangé, `+` fruit. La maison des fantômes, sa porte, les tunnels et les
positions de départ sont repérés automatiquement dans chaque carte ; les seuils
de l'arcade exprimés en nombre de points (sortie de la maison, Cruise Elroy,
fruits) sont ramenés au nombre de points de la carte.

| Touche | Action |
|---|---|
| Flèches, ZQSD ou WASD | diriger Pac-Man (le virage est mémorisé jusqu'à ce qu'il soit possible) |
| P ou Espace | pause |
| C | afficher les cases visées par les fantômes |
| I | IA marche / arrêt (une flèche reprend aussi la main) |
| N | nouvelle partie |
| X ou Échap | quitter |

## Les fantômes

Un fantôme ne choisit sa direction qu'en arrivant sur une case. Il ne fait
jamais demi-tour de lui-même, et prend la direction qui le rapproche le plus
(à vol d'oiseau) de sa **case cible**. En cas d'égalité, il préfère haut, gauche,
bas puis droite. Chaque fantôme a donc sa personnalité grâce à sa façon de
choisir sa cible :

| Fantôme | Poursuite | Dispersion |
|---|---|---|
| **Blinky** (rouge) | la case de Pac-Man | coin haut droit |
| **Pinky** (rose) | 4 cases devant Pac-Man (4 en haut **et** 4 à gauche s'il regarde vers le haut, bogue de l'arcade) | coin haut gauche |
| **Inky** (cyan) | prend la case 2 devant Pac-Man, puis double le vecteur qui va de Blinky à cette case : il prend Pac-Man en tenaille avec Blinky | coin bas droit |
| **Clyde** (orange) | Pac-Man s'il en est à plus de 8 cases, sinon son coin : il tourne autour de Pac-Man sans jamais l'attaquer franchement | coin bas gauche |

Autres règles reprises de l'arcade :
- **Cruise Elroy** : quand il reste peu de points, Blinky accélère (deux paliers) et poursuit Pac-Man même en dispersion.
- **Zones rouges** : au-dessus de la maison et du départ de Pac-Man, les fantômes ne peuvent pas monter.
- **Sortie de la maison** : Pinky sort tout de suite. Inky et Clyde sortent quand Pac-Man a mangé assez de points (30 et 60 au niveau 1). Si Pac-Man ne mange rien pendant 4 s, le fantôme suivant sort. Après une vie perdue, un compteur commun les libère à 7, 17 et 32 points.
- **Vitesses** : elles dépendent du niveau. Les fantômes ralentissent dans le tunnel et en effroi, et leurs yeux rentrent très vite.

## Les modes

| Mode | Comportement |
|---|---|
| **Dispersion** (*scatter*) | chaque fantôme rejoint son coin et tourne autour du pâté de murs voisin |
| **Poursuite** (*chase*) | chaque fantôme traque Pac-Man à sa manière (tableau ci-dessus) |
| **Effroi** (*frightened*) | après une énergie : fantômes bleus, lents, qui choisissent leur direction au hasard à chaque carrefour ; ils clignotent avant la fin ; on les mange pour 200, 400, 800 puis 1 600 points |
| **Mangé** (*eaten*) | les yeux retournent à la maison à grande vitesse pour s'y régénérer, par le plus court chemin (avec la règle de l'arcade, les yeux pouvaient tourner en rond indéfiniment dans certaines cartes de la version initiale) |

Dispersion et poursuite alternent selon un calendrier propre au niveau. Au
niveau 1, cela donne 7 s de dispersion, 20 s de poursuite, 7 s, 20 s, 5 s, 20 s,
5 s, puis poursuite définitive. L'effroi suspend ce calendrier. Chaque
changement de mode, et le début de l'effroi, fait faire demi-tour aux fantômes :
c'est ce qui permet de voir le changement à l'écran. Le panneau de droite
affiche en permanence le mode global, le mode et la cible de chaque fantôme,
ainsi qu'un journal des événements.

## Correction : sortie de la maison après l'effroi

**Bogue :** quand Pac-Man mange une énergie, les fantômes encore dans la maison
deviennent eux aussi effrayés. S'ils sortaient pendant l'effroi, ils restaient
effrayés. Ils gardaient aussi l'ancien mode qu'ils avaient avant d'entrer.

**Correction :** au moment où un fantôme franchit la porte, `sortie_terminee`
(`src/fantomes.c`) fait deux choses, même si l'effroi continue pour les autres
fantômes :
1. elle lui retire l'effroi ;
2. elle lui fait adopter le **mode global en cours** (dispersion ou poursuite).

Cette règle s'applique aussi à un fantôme mangé qui vient de se régénérer.

`make test` vérifie ce scénario précis. Pendant chaque partie simulée, il vérifie
aussi à chaque instant qu'aucun fantôme ne sort effrayé ou dans un ancien mode.

## L'IA de Pac-Man

Contrairement au projet, où Pac-Man ne voit que la carte et ne peut rien
mémoriser, l'IA dispose ici de l'état complet de la partie : directions,
modes et minuteries des fantômes, et même leur générateur pseudo-aléatoire.
Pour elle, le jeu est donc **entièrement prévisible**. Elle copie la partie et
fait tourner le vrai moteur pour connaître l'avenir exact de chaque suite de
coups.

1. **Recherche arborescente.** L'IA décide en arrivant sur chaque case. Elle
   explore toutes les suites de coups sur 16 cases. Elle ne fait demi-tour
   qu'au premier coup ou dans un cul-de-sac, si bien que l'arbre ne se ramifie
   qu'aux carrefours.
2. **Évaluation de chaque suite :**
   - être attrapé : exclu, sauf s'il n'y a rien d'autre ;
   - finir le niveau : excellent ;
   - sinon, les points réellement gagnés (points, fantômes, fruit), avec :
     - une pénalité si la position finale est un piège, c'est-à-dire si peu de
       cases sont atteignables avant les fantômes dangereux ;
     - un bonus pour se rapprocher d'un fantôme encore mangeable ;
     - une petite pénalité pour une énergie mangée sans fantôme à proximité ;
     - la distance au point le plus proche, qui sert seulement à départager.
3. **Garde-fous contre les hésitations.** Une recherche à horizon limité peut
   tourner en rond : la meilleure suite recule d'un pas à chaque décision.
   L'IA garde donc un peu de mémoire d'une case à l'autre :
   - un demi-tour lui coûte un peu (hystérésis) ;
   - après 30 décisions sans rien manger, elle devient « pressée » : chaque
     point mangé compte bien plus que la distance ;
   - après 120 décisions, elle suit directement le plus court chemin vers un
     point, en n'écartant que les coups mortels.
4. **Coût.** Environ 1 à 3 ms par décision.

**Résultats.** J'ai lancé 60 parties sur les 3 cartes de la version initiale
et 10 sur le plan de l'arcade, avec `--test` et plusieurs graines. L'IA a
terminé les trois niveaux dans les 70 parties, sans jamais perdre ses trois
vies. Elle marque environ 20 000 à 33 000 points, dont 25 à 39 fantômes
mangés.

## Organisation du code

| Fichier | Rôle |
|---|---|
| `src/labyrinthe.c` | lecture des cartes ASCII, plan de l'arcade, repérage de la maison, tunnels, zones rouges |
| `src/fantomes.c` | intelligence des fantômes : cibles, choix de direction, maison, correction de la sortie |
| `src/jeu.c` | règles générales : tables des niveaux, calendrier des modes, effroi, sorties, score, collisions |
| `src/ia.c` | IA de Pac-Man (recherche sur le moteur de jeu) |
| `src/rendu.c` | affichage ANSI (styles ASCII et blocs) |
| `src/terminal.c` | clavier en mode brut, écran alternatif |
| `src/main.c` | boucle principale (60 tics/s), options, tests |
