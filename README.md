# Pacman — Manuel de la logique du joueur

> Projet d'Algorithmique 1 — ENSSAT, Université de Rennes (F. Goasdoué).
> Ce document explique **comment concevoir l'IA de Pacman**. Il complète `projet.pdf`
> en détaillant ce que le sujet laisse implicite : le contrat exact de la fonction à
> écrire, la topologie réelle du plateau, les règles de mort, et une progression de
> stratégies du joueur aléatoire jusqu'à une exploration complète.
>
> Tout ce qui est affirmé ici sur le moteur a été **vérifié expérimentalement** contre
> le `pacman.o` fourni (voir [Annexe A](#annexe-a--comment-ces-faits-ont-été-vérifiés)).

---

## Sommaire

1. [Objectif du jeu et critère d'évaluation](#1-objectif-du-jeu-et-critère-dévaluation)
2. [Le contrat de la fonction `pacman`](#2-le-contrat-de-la-fonction-pacman)
3. [Le plateau : alphabet, repère, topologie](#3-le-plateau--alphabet-repère-topologie)
4. [Les trois façons de perdre](#4-les-trois-façons-de-perdre)
5. [La contrainte centrale : un joueur *sans mémoire*](#5-la-contrainte-centrale--un-joueur-sans-mémoire)
6. [Construire le joueur par paliers](#6-construire-le-joueur-par-paliers)
7. [Boîte à outils C99 et pièges](#7-boîte-à-outils-c99-et-pièges)
8. [Tester et mesurer](#8-tester-et-mesurer)
9. [Règles du sujet et checklist de rendu](#9-règles-du-sujet-et-checklist-de-rendu)
10. [Références](#10-références)
11. [Annexe A — comment ces faits ont été vérifiés](#annexe-a--comment-ces-faits-ont-été-vérifiés)

---

## 1. Objectif du jeu et critère d'évaluation

Pacman parcourt un labyrinthe. Chaque case non encore visitée qu'il traverse lui
rapporte des points ; les fantômes le poursuivent ; les énergisants inversent
temporairement le rapport de force.

| Événement | Effet |
|---|---|
| Traverser une case non explorée `.` | **+10 points** (`VIRGIN_PATH_SCORE`) |
| Traverser un énergisant `O` | **+50 points** (`ENERGY_SCORE`) et passage en *mode énergie* |
| Traverser une case déjà explorée ` ` | 0 point |
| Manger un fantôme (en mode énergie) | bonus (le moteur affiche `BONUS` / `Nextbonus`) |

**Ordre des priorités imposé par le sujet :**

1. **Finir le niveau** (explorer *toutes* les cases `.`) — c'est l'objectif principal ;
2. à défaut, aller le plus loin possible dans l'exploration ;
3. et, à performance égale, maximiser le score.

> Conséquence directe sur la conception : une IA qui **survit longtemps mais tourne en
> rond** vaut moins qu'une IA qui **explore méthodiquement**. Optimisez la couverture de
> la carte, pas la survie pour elle-même.

---

## 2. Le contrat de la fonction `pacman`

C'est le **seul** point d'entrée. Le moteur l'appelle une fois par tour et attend une
direction.

```c
direction pacman(
    char **map,                    // le labyrinthe, tableau de ysize chaînes de xsize caractères
    int xsize,                     // nombre de colonnes
    int ysize,                     // nombre de lignes
    int x,                         // colonne de Pacman
    int y,                         // ligne de Pacman
    direction lastdirection,       // direction jouée au tour précédent ; -1 au tout premier appel
    bool energy,                   // Pacman est-il en mode énergie ?
    int remainingenergymoderounds  // tours restants en mode énergie (si energy vaut true)
);
```

Le type de retour est défini dans `pacman_def.h` :

```c
enum compass { NORTH, EAST, SOUTH, WEST };   // soit 0, 1, 2, 3
typedef enum compass direction;
```

### Ce qu'il faut retenir du contrat

| Point | Détail |
|---|---|
| **Entrées en lecture seule** | `map` est une *copie* fournie par le moteur à chaque tour. Le modifier n'a aucun effet sur le jeu et n'est pas conservé d'un tour à l'autre. Ne comptez pas dessus comme mémoire. |
| **Une seule décision par appel** | Pas de déplacement en diagonale, pas de « rester sur place » : vous devez rendre `NORTH`, `EAST`, `SOUTH` ou `WEST`. |
| **`map[y][x]` vaut `'@'`** | Votre propre case contient le caractère `PACMAN`, **pas** son contenu d'origine. Traitez-la comme praticable dans vos parcours. |
| **`lastdirection == -1` au premier tour** | Voir le [piège de signe](#piège-2--lastdirection--1-oui-lastdirection--0-non) : `lastdirection == -1` fonctionne, `lastdirection < 0` **non**. |
| **Temps de calcul** | Le moteur ne fixe pas de budget explicite, mais il appelle la fonction à chaque tour. Un parcours en largeur sur une carte de 27×23 est instantané : ne vous bridez pas. |

---

## 3. Le plateau : alphabet, repère, topologie

### 3.1 L'alphabet ASCII

Les caractères sont déclarés `extern const char` dans `pacman_dec.h` et définis dans
`pacman.o`. **Utilisez toujours les constantes, jamais les littéraux** — c'est une
exigence de lisibilité, et cela vous protège si les valeurs changent.

| Constante | Valeur | Signification | Praticable ? |
|---|:---:|---|:---:|
| `WALL` | `*` | mur | ❌ **mort** |
| `DOOR` | `-` | porte de l'antre des fantômes | ❌ **mort** |
| `VIRGIN_PATH` | `.` | couloir non exploré | ✅ *+10 pts* |
| `PATH` | *espace* | couloir déjà exploré | ✅ 0 pt |
| `ENERGY` | `O` | énergisant | ✅ *+50 pts, mode énergie* |
| `PACMAN` | `@` | vous | ✅ (votre case) |
| `GHOST1` | `$` | Blinky | ✅ physiquement, mais ⚠️ |
| `GHOST2` | `%` | Inky | ✅ physiquement, mais ⚠️ |
| `GHOST3` | `#` | Pinky | ✅ physiquement, mais ⚠️ |
| `GHOST4` | `&` | Clyde | ✅ physiquement, mais ⚠️ |

> ⚠️ **Un fantôme masque le contenu de sa case.** Si `map[y][x] == GHOST1`, vous ne savez
> pas si la case dessous est `.` ou ` `. Pour vos parcours : une case-fantôme est
> *traversable* (ce n'est pas un mur) mais *dangereuse* hors mode énergie.

`pacman_dec.h` expose aussi `extern bool DEBUG`, positionné par l'option `-debug on` du
moteur. C'est le bon interrupteur pour vos `printf` de mise au point.

### 3.2 Le repère

```
        x croît vers l'EST  ──────────────►
   ┌────────────────────────────────────────┐
 y │  (0,0)                    (xsize-1, 0)  │   NORTH : y − 1
 c │                                         │   SOUTH : y + 1
 r │           map[y][x]                     │   EAST  : x + 1
 o │                                         │   WEST  : x − 1
 î │ (0, ysize-1)        (xsize-1, ysize-1)  │
 t └────────────────────────────────────────┘
```

**L'origine est en haut à gauche**, donc **`NORTH` décrémente `y`**. C'est l'inverse d'un
repère mathématique : c'est la source d'erreur n°1 sur ce projet. Écrivez les tables de
décalage une seule fois et servez-vous-en partout :

```c
static const int DX[4] = {  0, +1,  0, -1 };  /* NORTH, EAST, SOUTH, WEST */
static const int DY[4] = { -1,  0, +1,  0 };
```

### 3.3 Le plateau est un **tore** (et c'est important)

Sortir par un bord vous fait **réapparaître sur le bord opposé**. Ce n'est pas une
interprétation : c'est vérifié — depuis `(x=0, y=11)` du `level1.map`, renvoyer `WEST`
amène Pacman en `(x=26, y=11)`.

C'est le mécanisme des **tunnels** que vous voyez dans les niveaux, ici la ligne 11 de
`level1.map` :

```
..........**%#&**..........      ← les deux extrémités sont reliées
```

Le joueur aléatoire fourni encode déjà cette règle, de façon un peu déguisée :

```c
if (y == 0 || (y > 0 && map[y-1][x] != WALL && map[y-1][x] != DOOR))
    north = true;          //  ↑ « si je suis au bord nord, le nord est toujours permis »
```

**Conséquence pour vos algorithmes :** toute arithmétique de voisinage doit être
*modulaire*. Un voisin se calcule ainsi, et jamais autrement :

```c
int nx = (x + DX[d] + xsize) % xsize;   /* le + xsize est obligatoire, cf. piège 1 */
int ny = (y + DY[d] + ysize) % ysize;
```

Une distance de Manhattan doit elle aussi tenir compte du tore :

```
dist_x = min( |xa - xb| , xsize - |xa - xb| )
dist_y = min( |ya - yb| , ysize - |ya - yb| )
```

> **Note.** Dans les niveaux fournis, les lignes 0 et `ysize-1` sont entièrement des murs :
> le bouclage vertical existe dans la règle mais n'est jamais empruntable en pratique.
> Traitez quand même les deux axes de la même façon — cela ne coûte rien et vous protège
> d'un niveau de test différent.

---

## 4. Les trois façons de perdre

Le moteur ne connaît que trois fins prématurées (messages extraits de `pacman.o`) :

| Message du moteur | Cause |
|---|---|
| `RIP: you bumped into a wall!` | Vous avez renvoyé une direction vers un `WALL` ou une `DOOR`. |
| `RIP: you have just been eaten by a ghost!` | Un fantôme vous a atteint hors mode énergie. |
| `RIP: you have just been stuck by a ghost who is about to eat you!` | Vous vous êtes **enfermé** : toutes vos sorties mènent à un fantôme. |

Trois leçons de conception :

1. **La validation du coup est non négociable.** Avant tout `return`, vérifiez que la
   case visée (calculée *modulo*) n'est ni `WALL` ni `DOOR`. Une seule erreur = partie
   terminée au tour 1. C'est le bug le plus fréquent.
2. **Les fantômes tuent au contact**, y compris quand c'est *eux* qui viennent vers vous.
   Fuir « la case la plus loin d'un fantôme » ne suffit pas : il faut regarder où le
   fantôme peut être **au tour suivant**.
3. **Le troisième message existe** : être acculé dans un cul-de-sac est une mort à part
   entière. Une bonne IA évite d'entrer dans une impasse quand un fantôme est proche —
   préférez les **jonctions** (cases à ≥ 3 sorties), qui laissent des échappatoires.

---

## 5. La contrainte centrale : un joueur *sans mémoire*

C'est **la** difficulté du projet, et le sujet l'impose explicitement :

> « Votre code ne doit utiliser AUCUNE nouvelle variable globale, fichier sur disque ou
> autre moyen visant à stocker de l'information entre deux appels à la fonction `pacman`. »

Sont donc **interdits** : les variables globales, les `static` locales, les fichiers, les
variables d'environnement. Chaque appel repart de zéro.

### La solution : la carte *est* la mémoire

Le moteur marque lui-même votre passage. Observé sur trois tours consécutifs :

```
tour 0   *O..**.......@.......**..O*      Pacman part en (13,17)
tour 1   *O..**....... @......**..O*      la case quittée est devenue ' '
tour 2   *O..**.......  @.....**..O*      la trace persiste
```

Autrement dit, `map` encode **tout votre historique** sous une forme compacte :

* `' '` (`PATH`) = « je suis déjà passé ici » ;
* `'.'` (`VIRGIN_PATH`) = « il reste du travail ici ».

Vous n'avez donc pas besoin de mémoire : **vous pouvez la relire dans la carte à chaque
tour**. C'est ce qui rend le palier 3 ci-dessous possible et légal.

Ce que la carte ne vous dit *pas* (et que vous ne pouvez pas mémoriser) :

* le contenu d'une case masquée par un fantôme ;
* la direction dans laquelle un fantôme se déplace (vous avez sa position, pas son
  vecteur) ;
* votre propre trajectoire fine — mais `lastdirection` vous en donne le dernier pas, ce
  qui suffit pour interdire le demi-tour.

---

## 6. Construire le joueur par paliers

Chaque palier est jouable et testable. **Ne sautez pas d'étape** : validez le palier *n*
avant d'attaquer le *n+1*, sinon vous déboguerez deux choses à la fois.

### Palier 0 — le joueur aléatoire (fourni)

Il calcule les quatre praticabilités, puis tire au sort jusqu'à tomber sur une direction
valide.

**Ses deux défauts**, qui donnent la feuille de route :

* il fait demi-tour environ une fois sur deux et **piétine** ;
* il ne regarde ni les points, ni les fantômes.

> Le tirage `do { d = rand() % 4; } while (!ok);` est aussi une mauvaise habitude : on
> tire indéfiniment en espérant tomber juste. Préférez **construire la liste des
> directions valides** puis tirer un indice dedans — c'est borné et plus lisible.

### Palier 1 — interdire le demi-tour

Le gain est spectaculaire pour trois lignes de code. Le demi-tour de `d` est
`(d + 2) % 4` (avec l'ordre `NORTH, EAST, SOUTH, WEST`).

```
  candidats ← directions praticables
  si lastdirection ≠ -1 et candidats contient au moins une direction
     autre que l'opposé de lastdirection :
        retirer l'opposé de lastdirection des candidats
  retourner un candidat
```

La garde « au moins une autre » est indispensable : dans un cul-de-sac, le demi-tour est
le **seul** coup légal.

### Palier 2 — glouton local

Classez les quatre voisins par intérêt immédiat et prenez le meilleur :

```
  ENERGY (O)        ← si un fantôme est proche, ou si le mode énergie est fini
  VIRGIN_PATH (.)   ← le pain quotidien : +10
  PATH (' ')        ← se déplacer sans gagner, à n'utiliser qu'en dernier recours
```

À égalité, départagez au hasard (sinon l'IA devient déterministe et se bloque toujours au
même endroit).

**Limite de ce palier, et c'est une limite fondamentale :** quand Pacman est au milieu
d'une zone entièrement explorée, *tous* les voisins sont `' '`, le glouton n'a plus
d'information et redevient aléatoire. Il termine rarement un niveau. C'est précisément le
problème que résout le palier 3.

### Palier 3 — **exploration par parcours en largeur** (le cœur du projet)

> **Idée.** À chaque tour, chercher dans le labyrinthe **la case `.` la plus proche** et
> faire **un pas** vers elle. Recommencer au tour suivant.

C'est un parcours en largeur (*BFS*, [Breadth-First Search][bfs]) sur le graphe des cases
praticables, où chaque arête vaut 1 : le BFS donne le plus court chemin en nombre de pas.
Comme on le relance à chaque tour, la stratégie s'adapte naturellement aux fantômes qui
bougent et aux cases mangées entre-temps — pas besoin de stocker le chemin (ce qui serait
de toute façon interdit).

**Pseudo-code**

```
fonction prochain_pas_vers_case_non_exploree(map, xsize, ysize, x, y) :

    # 1. initialisation
    distance[.]  ← tableau xsize×ysize rempli de -1        (alloué avec malloc)
    premierpas[.] ← tableau xsize×ysize                    (la 1re direction du chemin)
    file ← file vide de capacité xsize×ysize               (file circulaire ou simple tableau)

    distance[y][x] ← 0
    enfiler (x, y)

    # 2. propagation
    tant que la file n'est pas vide :
        (cx, cy) ← défiler
        si map[cy][cx] = VIRGIN_PATH ou map[cy][cx] = ENERGY :
            retourner premierpas[cy][cx]        # cible atteinte, on rend le 1er pas
        pour d dans {NORTH, EAST, SOUTH, WEST} :
            nx ← (cx + DX[d] + xsize) mod xsize
            ny ← (cy + DY[d] + ysize) mod ysize
            si map[ny][nx] = WALL ou map[ny][nx] = DOOR : continuer
            si distance[ny][nx] ≠ -1 : continuer          # déjà visité
            distance[ny][nx]  ← distance[cy][cx] + 1
            premierpas[ny][nx] ← (distance[cy][cx] = 0) ? d : premierpas[cy][cx]
            enfiler (nx, ny)

    # 3. plus aucune case non explorée n'est atteignable
    retourner AUCUNE
```

**Points de méthode**

* **Rendre le premier pas, pas le chemin.** L'astuce `premierpas` évite de reconstruire
  le chemin à rebours : on propage la direction initiale le long du BFS. C'est plus simple
  et plus court qu'un tableau de prédécesseurs. (L'alternative — stocker `prev` puis
  remonter depuis la cible — est tout aussi correcte ; choisissez celle que vous saurez
  expliquer au chargé de TP.)
* **La case de départ ne déclenche pas l'arrêt** : `map[y][x]` vaut `'@'`, donc le test
  `VIRGIN_PATH` échoue naturellement au tour 0. Pas de cas particulier à écrire.
* **Complexité** : `O(xsize × ysize)` en temps et en mémoire par appel. Sur 27×23 = 621
  cases, c'est négligeable, même appelé à chaque tour.
* **Le cas « file vide »** signifie qu'il ne reste plus aucune case `.` joignable :
  soit le niveau est fini, soit une zone est inaccessible. Prévoyez un repli (palier 1).

Avec ce seul palier, et en ignorant les fantômes, un Pacman termine régulièrement un
niveau — dès lors qu'il ne se fait pas manger.

### Palier 4 — intégrer les fantômes

Le BFS du palier 3 est déjà l'outil ; il suffit de l'utiliser deux fois.

**a) Une carte de danger.** Lancez un BFS *depuis les fantômes* (BFS multi-sources :
enfilez les quatre positions de fantômes avec `distance = 0`). Vous obtenez pour chaque
case sa distance au fantôme le plus proche, **en nombre de pas réels**, murs compris.
C'est incomparablement plus juste qu'une distance à vol d'oiseau, qui traverse les murs.

**b) Une décision par arbitrage.** À chaque tour :

```
  si energy = vrai et remainingenergymoderounds > marge :
        cible ← le fantôme joignable le plus proche      # chasse : ils rapportent un bonus
  sinon si danger[case visée] ≤ SEUIL :
        choisir le coup qui maximise danger[.]           # fuite
  sinon :
        cible ← la case non explorée la plus proche       # exploration (palier 3)
```

**Réglages à travailler, et à savoir justifier :**

* `SEUIL` : à partir de quelle distance un fantôme est-il « proche » ? 2 ou 3 pas est un
  bon point de départ. Trop grand, Pacman devient peureux et n'explore plus.
* `marge` : ne partez pas en chasse avec 2 tours d'énergie restants — vous arriveriez sur
  le fantôme au moment où il redevient mortel. Comparez `remainingenergymoderounds` à la
  **distance BFS** vers la cible : ne chassez que si `distance + sécurité <
  remainingenergymoderounds`.
* **Les énergisants sont une ressource stratégique.** Il est souvent meilleur de garder un
  `O` en réserve près d'une zone dangereuse que de le manger dès qu'on le croise.
* **Préférez les jonctions en fuite.** Une case à 3 ou 4 sorties vaut mieux qu'un couloir,
  même légèrement plus éloigné du fantôme : c'est l'antidote au message
  *« stuck by a ghost »*.

> **Comment se comportent les fantômes ?** Le moteur expose une option `-mode easy|original`.
> En `original`, les quatre fantômes ont des personnalités distinctes (le classique
> Blinky/Pinky/Inky/Clyde), et le moteur calcule des distances **euclidiennes** — le
> symbole `sqrt` est présent dans `pacman.o`. Pour comprendre ces comportements, la
> référence est *The Pac-Man Dossier* ([§ Ghost Behavior][dossier]) : Blinky vous vise
> directement, Pinky vise *devant* vous, Inky combine la position de Blinky et la vôtre,
> Clyde alterne poursuite et fuite selon sa distance. Développez et validez votre IA en
> `easy`, puis vérifiez-la en `original`.

---

## 7. Boîte à outils C99 et pièges

### Piège 1 — le modulo négatif

En C, `-1 % 27` vaut **`-1`**, pas `26`. Écrire `(x - 1) % xsize` produit donc un indice
négatif et un accès mémoire hors tableau (comportement indéfini, plantage ou pire :
résultat silencieusement faux). **Toujours ajouter la taille avant le modulo :**

```c
int nx = (x + DX[d] + xsize) % xsize;   /* ✅ */
int nx = (x + DX[d]) % xsize;           /* ❌ */
```

### Piège 2 — `lastdirection == -1` oui, `lastdirection < 0` non

`enum compass` n'a que des valeurs positives ; GCC lui attribue donc un type sous-jacent
**non signé**. Vérifié sur cette machine :

```
sizeof(direction) = 4,  type non signé
lastdirection == -1   →  vrai   ✅   (les deux opérandes sont convertis, la comparaison marche)
lastdirection <  0    →  FAUX   ❌   (une valeur non signée n'est jamais négative)
```

Testez le premier tour avec `lastdirection == -1`, jamais avec un test de signe. Et n'utilisez
jamais `lastdirection` comme indice de tableau sans avoir écarté ce cas : `DX[lastdirection]`
au premier tour, c'est un accès hors bornes.

### Piège 3 — allouer sans variable globale

Vos tableaux de travail (distances, file) doivent être alloués **à chaque appel** et
libérés avant chaque `return`. Un `malloc` sans `free` dans une fonction appelée des
milliers de fois, c'est une fuite qui grossit toute la partie.

```c
int *dist = malloc((size_t)xsize * ysize * sizeof *dist);
if (dist == NULL) return /* un repli sûr */;   /* toujours tester malloc */
...
free(dist);
```

Le sujet interdit les variables globales, **pas** les paramètres : passez `map`, `xsize`,
`ysize` et vos tableaux de travail en arguments de vos sous-fonctions. C'est d'ailleurs ce
que le sujet attend — un code découpé en modules, chacun commenté.

> Un tableau automatique de taille variable (`int dist[ysize][xsize];`, un *VLA* C99) est
> une alternative légale et plus simple. Il vit sur la pile : à 621 cases c'est sans
> risque, mais `malloc` reste le choix défendable et portable.

### Piège 4 — se déclarer praticable soi-même

Dans vos tests « est-ce que je peux aller là ? », la condition est **exactement** :

```c
map[ny][nx] != WALL && map[ny][nx] != DOOR
```

Ne testez pas « est-ce que c'est `PATH` ou `VIRGIN_PATH` » : vous exclueriez les
énergisants et les cases occupées par un fantôme, qui sont bien praticables.

### Piège 5 — le déterminisme

Sans aucun aléa, une IA purement gloutonne se coince dans une boucle et rejoue
indéfiniment le même cycle. Gardez un tirage au sort pour **départager les ex æquo** (et
seulement pour ça). `rand()` suffit ; le moteur appelle déjà `srand`.

### Structure de code attendue

Le sujet est explicite : **tout dans `player.c`**, et chaque sous-module doit porter
**deux** commentaires :

1. **avant** le module : la *stratégie* — quelle sous-question il résout, et l'idée
   générale de la méthode ;
2. **dans** le module : la *mise en œuvre* — pourquoi ce test, pourquoi cette boucle.

Un découpage naturel, qui suit ce manuel :

```
player.c
├── pacman()                     ← l'arbitrage : chasse / fuite / exploration
├── voisin()                     ← (x, y, d) → coordonnées du voisin, modulo le tore
├── praticable()                 ← WALL et DOOR exclus
├── est_fantome()                ← teste les 4 constantes GHOST*
├── bfs_vers_non_explore()       ← palier 3 : renvoie le 1er pas, ou AUCUNE
├── bfs_depuis_fantomes()        ← palier 4a : remplit la carte de danger
└── directionprinter()           ← déjà présent, pour le mode -debug on
```

---

## 8. Tester et mesurer

### Compilation

La commande du sujet :

```sh
gcc -std=c99 -Wall -o pacman pacman.o player.c
```

Selon le `pacman.o` fourni, il peut manquer la bibliothèque mathématique — ajoutez `-lm`
en fin de ligne si l'éditeur de liens réclame `sqrt` :

```sh
gcc -std=c99 -Wall -o pacman pacman.o player.c -lm
```

> ⚠️ Le `pacman.o` de ce dépôt est un objet **Linux (ELF)** : il ne peut pas être lié par
> un GCC Windows natif. Compilez sous **WSL** (ou sous Linux).

### Options du moteur

```
pacman [-debug on/off] [-display color/bw] [-delay entier] [-mode easy/original] fichier_niveau
```

| Option | Usage |
|---|---|
| `-debug on` | met `DEBUG` à `true` : vos traces s'affichent |
| `-display bw` | noir et blanc, utile pour rediriger la sortie dans un fichier |
| `-delay N` | pause entre deux tours, en microsecondes. **`-delay 0` fait planter le moteur** (division par zéro) : utilisez `1` pour aller vite |
| `-mode original` | fantômes avec leurs personnalités classiques (plus dur que `easy`) |

### Méthode de test

* **Lancer sans argument** affiche l'aide — le sujet recommande de commencer par là.
* **Une seule partie ne prouve rien.** Le jeu comporte de l'aléa. Faites tourner
  *n* parties par niveau et comparez les moyennes : taux de victoire d'abord, score ensuite.
* **Mesurez avant et après** chaque palier. Un palier qui n'améliore pas le taux de
  victoire est un palier à revoir ou à jeter — et c'est exactement le genre de chiffre que
  le chargé de TP appréciera de vous voir citer.
* **Vérifiez la mémoire.** Sous Linux/WSL, `valgrind ./pacman -delay 1 levels/level1.map`
  ou une compilation avec `-fsanitize=address,undefined` détectent immédiatement un
  `malloc` non libéré ou un indice hors bornes — typiquement une erreur de modulo.
* Le moteur écrit un journal `pacman.csv` : pratique pour tracer l'évolution du score.

> La branche `build/makefile` de ce dépôt ajoute un `Makefile` qui enveloppe tout cela :
> `make run`, `make debug`, `make check` (compilation en `-Werror` + vérification que les
> `.h` fournis n'ont pas été modifiés), `make test` (parties en série avec taux de
> victoire), `make asan`, `make rendu`. Voir `make help`.

---

## 9. Règles du sujet et checklist de rendu

**Interdits** — le non-respect est sanctionné :

- ❌ Modifier `pacman_def.h`, `pacman_dec.h` ou `player.h`.
- ❌ Créer un fichier source supplémentaire : **tout** le code va dans `player.c`.
- ❌ Toute variable globale nouvelle, tout fichier, tout moyen de persister entre deux
  appels à `pacman`.
- ❌ Toute bibliothèque hors bibliothèque standard du compilateur C.
- ❌ Du code qui ne soit pas entièrement le vôtre — un détecteur de plagiat est passé sur
  l'ensemble de la promotion.

**Checklist avant dépôt sur Moodle :**

- [ ] `student_name` renseigné avec votre nom complet.
- [ ] Compilation **sans aucun warning** : `gcc -std=c99 -Wall -Wextra -Werror ...`.
- [ ] Code parfaitement indenté.
- [ ] Chaque module commenté **deux fois** (stratégie avant, mise en œuvre dedans).
- [ ] Aucun `malloc` sans `free`.
- [ ] Le fichier est renommé **`nom-prenom.c`** — l'oublier coûte **−3 points**,
      quelle que soit la note du chargé de TP.
- [ ] Vous savez **expliquer chaque ligne** : l'évaluation est un oral de 5 minutes où
      l'on vous demandera de montrer le code correspondant à chaque question.

---

## 10. Références

| Sujet | Référence |
|---|---|
| Le jeu original | [Pac-Man — Wikipédia (EN)](https://en.wikipedia.org/wiki/Pac-Man) — cité par le sujet |
| **Comportement des fantômes** | [*The Pac-Man Dossier*, Jamey Pittman][dossier] — la référence exhaustive sur l'IA des quatre fantômes, leurs cibles et les phases *scatter*/*chase* |
| Parcours en largeur | [Breadth-first search — Wikipédia (EN)][bfs] ; Cormen, Leiserson, Rivest, Stein, *Introduction to Algorithms*, ch. 22.2 |
| Remplissage par diffusion | [Flood fill — Wikipédia (EN)](https://en.wikipedia.org/wiki/Flood_fill) — la même idée, vue comme un coloriage |
| Recherche de chemin, vue d'ensemble | [Red Blob Games — *Introduction to A\**](https://www.redblobgames.com/pathfinding/a-star/introduction.html) — explications interactives de BFS, Dijkstra et A\* |
| Modulo négatif en C | ISO/IEC 9899:1999 (C99), § 6.5.5 al. 6 : la division entière tronque vers zéro |
| Type sous-jacent d'un `enum` | ISO/IEC 9899:1999 (C99), § 6.7.2.2 al. 4 : le choix est laissé à l'implémentation |

[dossier]: https://pacman.holenet.info/
[bfs]: https://en.wikipedia.org/wiki/Breadth-first_search

---

## Annexe A — comment ces faits ont été vérifiés

Les affirmations de ce manuel qui ne figurent pas dans `projet.pdf` ont été établies en
liant un `player.c` instrumenté (jetable) contre le `pacman.o` de ce dépôt, sous WSL :

| Affirmation | Méthode |
|---|---|
| `VIRGIN_PATH_SCORE = 10`, `ENERGY_SCORE = 50` | affichage direct des constantes `extern` |
| Valeurs des 10 caractères du tableau § 3.1 | idem |
| `map[y][x]` vaut `'@'` | affichage de la case courante au tour 0 |
| Les cases quittées deviennent `' '` | affichage de `map[y]` sur trois tours consécutifs |
| **Le plateau boucle** | joueur dirigé en `(0, 11)` de `level1.map` renvoyant `WEST` → position suivante `(26, 11)` |
| Les trois messages de mort | chaînes lisibles extraites de `pacman.o` |
| Fantômes en distance euclidienne | présence du symbole `sqrt` dans les dépendances de `pacman.o` |
| `direction` est non signé | programme de test sur `sizeof` et comparaisons |

Ces fichiers de test étaient temporaires et ne font pas partie du dépôt. Si un `pacman.o`
différent vous est fourni (autre OS, autre année), revérifiez les valeurs du § 3.1 : le
code doit de toute façon passer par les constantes, jamais par les littéraux.
