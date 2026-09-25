# Pac-Man (arcade) dans le terminal

Version complète et indépendante du projet d'algorithmique : on y joue au
clavier, et les quatre fantômes suivent les règles de la borne d'arcade de 1980.

## Lancer

Linux, WSL ou macOS, terminal d'au moins **120 × 33** (le panneau d'informations
passe sous le labyrinthe si le terminal est plus étroit).

```sh
make          # compile ./pacman
make run      # jouer
make demo     # regarder le pilote automatique
make test     # vérifications automatiques (cibles, modes, sortie de la maison)
```

Options : `--niveau N`, `--vitesse F` (1 = vitesse de l'arcade), `--graine N`,
`--test N [--verbeux]`.

| Touche | Action |
|---|---|
| Flèches, ZQSD ou WASD | diriger Pac-Man (le virage est mémorisé jusqu'à ce qu'il soit possible) |
| P ou Espace | pause |
| C | afficher les cases visées par les fantômes |
| I | pilote automatique (IA) marche / arrêt |
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
| **Mangé** (*eaten*) | les yeux retournent à la maison à grande vitesse pour s'y régénérer |

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

## Organisation du code

| Fichier | Rôle |
|---|---|
| `src/labyrinthe.c` | plan du labyrinthe, tunnel, zones rouges |
| `src/fantomes.c` | intelligence des fantômes : cibles, choix de direction, maison, correction de la sortie |
| `src/jeu.c` | règles générales : tables des niveaux, calendrier des modes, effroi, sorties, score, collisions |
| `src/pilote.c` | pilote automatique (démo et tests) |
| `src/rendu.c` | affichage ANSI |
| `src/terminal.c` | clavier en mode brut, écran alternatif |
| `src/main.c` | boucle principale (60 tics/s), options, tests |
