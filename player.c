// add the needed C libraries below
#include <stdbool.h> // boolean logic
#include <stdlib.h>  // malloc, free
#include <stdio.h>   // printf (debug)

// look at the file below for the definition of the direction type
// .h must not be modified!
#include "pacman_dec.h"
#include "pacman_def.h"

// put the student names below (mandatory)
const char *student_name = "Ezequiel FRIDEL ESCALONA";

/*
 * Paramètres de l'IA (énumération : aucune variable globale n'est créée).
 *  - PROFONDEUR ...... nombre de coups de Pacman simulés à l'avance ;
 *  - NB_FANTOMES ..... nombre de fantômes du jeu ;
 *  - MAX_ETATS ....... nombre maximal d'états possibles mémorisés par fantôme ;
 *  - INFINI .......... distance d'une case inaccessible ;
 *  - PORTEE_CIBLE .... décalage (en cases) de la cible des fantômes 2 et 3 ;
 *  - ESPACE_MAX ...... nombre de cases sûres au-delà duquel une position est
 *                      considérée comme « non piégée » (plafond de l'espace libre) ;
 *  - MENACE .......... distance d'un fantôme en dessous de laquelle Pacman se
 *                      sent menacé (les énergies deviennent alors précieuses) ;
 *  - DETOUR_ENERGIE .. détour (en cases) accepté pour garder une énergie en
 *                      réserve quand aucun fantôme ne menace ;
 *  - CIBLE_RISQUEE ... pénalité (en cases) d'une cible atteignable uniquement
 *                      par des cases non sûres ;
 *  - CIBLE_MAX ....... distance retenue quand il n'y a plus aucune cible ;
 *  - BONUS_CHASSE .... raccourci (en cases) accordé à un fantôme mangeable ;
 *  - BRUIT ........... amplitude d'un petit bonus aléatoire ajouté à chaque coup
 *                      possible, pour ne pas répéter indéfiniment le même circuit ;
 *  - les SEUIL_... sont des probabilités, en millièmes ;
 *  - les poids P_... servent à noter une suite de coups.
 */
enum
{
	PROFONDEUR = 10,
	NB_FANTOMES = 4,
	MAX_ETATS = 400,
	INFINI = 1000000,
	PORTEE_CIBLE = 10,
	ESPACE_MAX = 40,
	MENACE = 7,
	DETOUR_ENERGIE = 25,
	CIBLE_RISQUEE = 50,
	CIBLE_MAX = 1000,
	BONUS_CHASSE = 30,
	BRUIT = 200,
	SEUIL_SURVIE = 1,	 // survie en dessous de laquelle on arrête de simuler (0,1 %)
	SEUIL_PRESENCE = 10, // probabilité à partir de laquelle un fantôme compte dans l'évaluation (1 %)
	P_POINT = 1000,			  // manger un point
	P_ENERGIE_REFUGE = 1500,  // manger une énergie quand un fantôme menace
	P_ENERGIE_RESERVE = -300, // manger une énergie sans nécessité (gaspillage)
	P_FANTOME = 30000,		  // manger un fantôme (il rapporte au moins autant que 20 points)
	P_TOUR = 10,			  // chaque tour d'attente avant un gain le diminue d'autant
	P_ESPACE = 500,			  // chaque case d'espace libre manquante
	P_CIBLE = 10,			  // chaque case de distance à la prochaine cible
	P_MORT = -50000000,		  // être mangé
	P_MORT_TOUR = 1000000	  // ... un peu moins grave pour chaque tour gagné avant
};

/*
 * État possible d'un fantôme. Pacman ne peut rien mémoriser d'un tour à
 * l'autre : il ignore donc la dernière direction des fantômes (qui guide leurs
 * déplacements) et le mode de jeu (easy ou original). Il raisonne alors sur
 * tous les états possibles, chacun ayant une probabilité.
 */
struct etat
{
	int cellule; // position (indice y * xsize + x)
	int dir;	 // dernière direction du fantôme (-1 : aucune, au début du jeu)
	int cible;	 // 1 : le fantôme vise une case (mode original), 0 : il erre au hasard
	double p;	 // probabilité de cet état
};

// Ensemble des états possibles d'un fantôme.
struct fantome
{
	int numero;				  // 1 à 4 (GHOST1 à GHOST4) : fixe son comportement
	int nb;					  // nombre d'états
	struct etat e[MAX_ETATS]; // les états
};

// Les fantômes à un instant de la simulation.
struct groupe
{
	int nb;						   // nombre de fantômes présents sur la carte
	struct fantome f[NB_FANTOMES]; // chacun d'eux
};

/*
 * Contexte de la recherche : données communes à toutes les fonctions (carte,
 * mode énergie, tableaux de travail). Il est créé dans pacman() et détruit à la
 * fin de l'appel : aucune information n'est conservée entre deux tours.
 */
struct contexte
{
	char **map;					// la carte
	int xsize, ysize, n;		// ses dimensions et son nombre de cases
	bool energy;				// mode énergie au début du tour
	int remaining;				// tours d'énergie restants au début du tour
	bool menace;				// un fantôme dangereux est-il proche ?
	bool reste_points;			// reste-t-il des points ('.') sur la carte ?
	int rayon_fuite2;			// carré du rayon de fuite des fantômes 1 à 3 en mode énergie
	int *padj;					// padj[4c + d] : case atteinte par Pacman depuis c dans la direction d (-1 : interdit)
	int *gadj;					// gadj[4c + d] : même chose pour un fantôme (qui franchit la porte)
	int *dist;					// distances de Pacman (BFS)
	int *dfant;					// distances des fantômes (BFS)
	int *file;					// file des BFS
	int *index;					// index[clé d'un état] : sa place dans l'ensemble en construction (-1 : absent)
	int nb_cibles;				// nombre de cases contenant un point ou une énergie
	int chemin[PROFONDEUR + 1]; // cases mangées le long de la suite de coups simulée
	int nb_chemin;				// nombre de cases dans chemin
	struct groupe *niveaux;		// niveaux[t] : fantômes après t coups simulés
	struct fantome *tampon;		// ensemble d'états temporaire
};

// put the prototypes of your additional functions/procedures below
static bool est_fantome(char c);
static char contenu(const struct contexte *ctx, int c);
static int voisin(const struct contexte *ctx, int c, direction d);
static bool preparer(struct contexte *ctx);
static void liberer(struct contexte *ctx);
static void placer_fantomes(struct contexte *ctx, bool debut);
static bool energise(const struct contexte *ctx, int k);
static bool voit(const struct contexte *ctx, int g, direction d, int pac);
static int choix_fantome(const struct contexte *ctx, int numero, const struct etat *s, int pac,
						 direction pdir, bool ener, int dirs[4], double probas[4]);
static void ajouter(struct contexte *ctx, struct fantome *f, int cellule, int dir, int cible, double p);
static void terminer(struct contexte *ctx, const struct fantome *f);
static void avancer(struct contexte *ctx, const struct fantome *src, struct fantome *dst, int pac,
					direction pdir, bool ener);
static double retirer(struct fantome *f, int cellule, bool normaliser);
static bool deja_mange(const struct contexte *ctx, int c);
static double chercher(struct contexte *ctx, int pac, int t, double survie, double gain, int arrivee,
					   direction *meilleure);
static double jouer_coup(struct contexte *ctx, int pac, direction d, int t, double survie, double gain);
static void distances_fantomes(struct contexte *ctx, const struct groupe *g);
static double evaluer_feuille(struct contexte *ctx, int pac, int t);
static void directionprinter(direction d);

/*
 * Fonction pacman : choix du prochain mouvement.
 *
 * Stratégie : recherche arborescente « expectimax » à horizon limité.
 *   1. Les règles de déplacement des fantômes sont connues (poursuite à vue,
 *      errance au hasard ou visée d'une case selon le fantôme et le mode, pas
 *      de demi-tour dans un couloir...). Ce qui est inconnu (dernière direction
 *      de chaque fantôme, mode de jeu, tirages au hasard) est représenté par un
 *      ensemble d'états possibles pondérés par leur probabilité.
 *   2. On simule toutes les suites de PROFONDEUR coups de Pacman. À chaque coup,
 *      les ensembles d'états des fantômes avancent selon leurs règles, et l'on
 *      calcule la probabilité que Pacman soit mangé.
 *   3. Chaque suite reçoit une valeur « espérée » : gains (points, énergies,
 *      fantômes) pondérés par la probabilité d'être encore en vie, énorme
 *      pénalité pondérée par la probabilité de mourir, et évaluation de la
 *      position finale (espace libre sûr et distance à la cible suivante).
 *   4. On joue le premier coup de la meilleure suite.
 * Aucune information n'est conservée entre deux tours : tout est recalculé.
 */
direction pacman(
	char **map,					  // the map as a dynamic array of strings, ie of arrays of chars
	int xsize,					  // number of columns of the map
	int ysize,					  // number of lines of the map
	int x,						  // x-position of pacman in the map
	int y,						  // y-position of pacman in the map
	direction lastdirection,	  // last move made by pacman (see pacman.h for the direction type; lastdirection value is -1 at the beginning of the game
	bool energy,				  // is pacman in energy mode?
	int remainingenergymoderounds // number of remaining rounds in energy mode, if energy mode is true
)
{
	struct contexte ctx;
	int pac = y * xsize + x;										   // position de Pacman
	bool debut = ((int)lastdirection < NORTH || lastdirection > WEST); // premier tour de la partie ?
	direction meilleure;

	ctx.map = map;
	ctx.xsize = xsize;
	ctx.ysize = ysize;
	ctx.n = xsize * ysize;
	ctx.energy = energy;
	ctx.remaining = remainingenergymoderounds;
	ctx.nb_chemin = 0;

	// Étape 0 : tableaux de travail. Sans mémoire, on joue un coup possible quelconque.
	if (!preparer(&ctx))
	{
		for (meilleure = NORTH; meilleure < WEST; meilleure++)
			if (contenu(&ctx, voisin(&ctx, pac, meilleure)) != WALL &&
				contenu(&ctx, voisin(&ctx, pac, meilleure)) != DOOR)
				break;
		return meilleure;
	}

	// Étape 1 : états initiaux des fantômes, puis détection d'une menace
	// (un fantôme dangereux à moins de MENACE cases de Pacman).
	placer_fantomes(&ctx, debut);
	distances_fantomes(&ctx, &ctx.niveaux[0]);
	ctx.menace = !energise(&ctx, MENACE) && ctx.dfant[pac] <= MENACE;

	// Étapes 2 à 4 : recherche de la meilleure suite de coups. La direction
	// précédente est proposée en premier : elle est gardée en cas d'égalité.
	meilleure = debut ? NORTH : lastdirection;
	chercher(&ctx, pac, 0, 1.0, 0.0, -1, &meilleure);

	liberer(&ctx);

	if (DEBUG)
	{
		printf("Next direction: ");
		directionprinter(meilleure);
		printf("\n");
	}

	// answer to the game engine
	return meilleure;
}

// the code of your additional functions/procedures must be put below

/*
 * est_fantome : indique si le caractère c représente l'un des 4 fantômes.
 */
static bool est_fantome(char c)
{
	return c == GHOST1 || c == GHOST2 || c == GHOST3 || c == GHOST4;
}

/*
 * contenu : renvoie le caractère de la carte situé à l'indice c = y * xsize + x.
 */
static char contenu(const struct contexte *ctx, int c)
{
	return ctx->map[c / ctx->xsize][c % ctx->xsize];
}

/*
 * voisin : renvoie l'indice de la case voisine de c dans la direction d.
 * Stratégie : on décale d'une case, puis on applique le « tore » de la carte :
 * sortir par un bord fait réapparaître du côté opposé (tunnels des niveaux).
 */
static int voisin(const struct contexte *ctx, int c, direction d)
{
	int x = c % ctx->xsize, y = c / ctx->xsize;

	// Décalage selon la direction : le nord est en haut (y décroissant).
	switch (d)
	{
	case NORTH: y--; break;
	case EAST:  x++; break;
	case SOUTH: y++; break;
	case WEST:  x--; break;
	}
	// Passage par les bords : on revient de l'autre côté.
	x = (x + ctx->xsize) % ctx->xsize;
	y = (y + ctx->ysize) % ctx->ysize;
	return y * ctx->xsize + x;
}

/*
 * preparer : alloue les tableaux de travail et précalcule les déplacements.
 * Stratégie : pour chaque case et chaque direction, on range une fois pour
 * toutes la case d'arrivée :
 *   - pour Pacman (padj) : interdite (-1) si c'est un mur ou la porte ;
 *   - pour un fantôme (gadj) : interdite si c'est un mur ; s'il s'agit de la
 *     porte, le fantôme la traverse d'un coup et arrive sur la case suivante
 *     (règle du moteur, sans passage par les bords de la carte).
 * On calcule aussi le rayon de fuite des fantômes 1 à 3 en mode énergie : la
 * partie entière du quart de la diagonale de la carte (racine carrée entière,
 * calculée par une simple boucle pour se passer de la bibliothèque math).
 * Renvoie false (tout étant libéré) si la mémoire manque.
 */
static bool preparer(struct contexte *ctx)
{
	int c, d, r, diag2;

	ctx->padj = malloc(4 * ctx->n * sizeof(int));
	ctx->gadj = malloc(4 * ctx->n * sizeof(int));
	ctx->dist = malloc(ctx->n * sizeof(int));
	ctx->dfant = malloc(ctx->n * sizeof(int));
	ctx->file = malloc(ctx->n * sizeof(int));
	ctx->index = malloc(ctx->n * 10 * sizeof(int));
	ctx->niveaux = malloc((PROFONDEUR + 1) * sizeof(struct groupe));
	ctx->tampon = malloc(sizeof(struct fantome));
	if (ctx->padj == NULL || ctx->gadj == NULL || ctx->dist == NULL || ctx->dfant == NULL ||
		ctx->file == NULL || ctx->index == NULL || ctx->niveaux == NULL || ctx->tampon == NULL)
	{
		liberer(ctx);
		return false;
	}

	for (c = 0; c < ctx->n * 10; c++)
		ctx->index[c] = -1;

	// Déplacements possibles et décompte des cibles (points et énergies).
	ctx->nb_cibles = 0;
	ctx->reste_points = false;
	for (c = 0; c < ctx->n; c++)
	{
		char car = contenu(ctx, c);
		if (car == VIRGIN_PATH || car == ENERGY)
		{
			ctx->nb_cibles++;
			if (car == VIRGIN_PATH)
				ctx->reste_points = true;
		}
		for (d = NORTH; d <= WEST; d++)
		{
			int v = voisin(ctx, c, d);
			char cv = contenu(ctx, v);
			ctx->padj[4 * c + d] = (car == WALL || cv == WALL || cv == DOOR) ? -1 : v;
			ctx->gadj[4 * c + d] = (car == WALL || cv == WALL) ? -1 : v;
			if (car != WALL && cv == DOOR)
			{
				// Franchissement de la porte : une case de plus, sans passer les bords.
				int x = v % ctx->xsize + (d == EAST) - (d == WEST);
				int y = v / ctx->xsize + (d == SOUTH) - (d == NORTH);
				ctx->gadj[4 * c + d] = (x < 0 || y < 0 || x >= ctx->xsize || y >= ctx->ysize ||
										ctx->map[y][x] == WALL || ctx->map[y][x] == DOOR)
										   ? -1
										   : y * ctx->xsize + x;
			}
		}
	}

	// Rayon de fuite : r = partie entière de la racine de la diagonale au carré.
	diag2 = ctx->xsize * ctx->xsize + ctx->ysize * ctx->ysize;
	for (r = 0; (r + 1) * (r + 1) <= diag2; r++)
		;
	// Un fantôme fuit si (int)distance <= r / 4, soit distance² < (r / 4 + 1)².
	ctx->rayon_fuite2 = (r / 4 + 1) * (r / 4 + 1);
	return true;
}

/*
 * liberer : libère tous les tableaux de travail (free(NULL) ne fait rien).
 */
static void liberer(struct contexte *ctx)
{
	free(ctx->padj);
	free(ctx->gadj);
	free(ctx->dist);
	free(ctx->dfant);
	free(ctx->file);
	free(ctx->index);
	free(ctx->niveaux);
	free(ctx->tampon);
}

/*
 * placer_fantomes : construit les états initiaux des fantômes (niveaux[0]).
 * Stratégie : la position de chaque fantôme est lue sur la carte, mais sa
 * dernière direction est inconnue. On retient comme possibles les directions
 * par lesquelles il a pu arriver sur sa case (la case d'où il viendrait n'est
 * pas un mur), toutes équiprobables ; au premier tour de la partie, elle vaut
 * -1 pour tous. Le mode de jeu étant lui aussi inconnu, les fantômes 1 à 3
 * ont une chance sur deux d'errer au hasard (mode easy) et une chance sur deux
 * de viser une case (mode original) ; le fantôme 4 erre toujours au hasard.
 */
static void placer_fantomes(struct contexte *ctx, bool debut)
{
	struct groupe *g = &ctx->niveaux[0];
	int c, d;

	g->nb = 0;
	for (c = 0; c < ctx->n && g->nb < NB_FANTOMES; c++)
	{
		char car = contenu(ctx, c);
		struct fantome *f = &g->f[g->nb];
		int nb_dirs = 0, cible;

		if (!est_fantome(car))
			continue;
		g->nb++;
		f->nb = 0;
		f->numero = (car == GHOST1) ? 1 : (car == GHOST2) ? 2 : (car == GHOST3) ? 3 : 4;

		// Nombre de directions d'arrivée possibles (arriver en allant vers d,
		// c'est venir de la case voisine dans la direction opposée d + 2).
		for (d = NORTH; d <= WEST; d++)
			if (ctx->gadj[4 * c + (d + 2) % 4] >= 0)
				nb_dirs++;

		for (cible = 0; cible <= (f->numero == 4 ? 0 : 1); cible++)
		{
			double pmode = (f->numero == 4) ? 1.0 : 0.5;
			if (debut || nb_dirs == 0)
				ajouter(ctx, f, c, -1, cible, pmode);
			else
				for (d = NORTH; d <= WEST; d++)
					if (ctx->gadj[4 * c + (d + 2) % 4] >= 0)
						ajouter(ctx, f, c, d, cible, pmode / nb_dirs);
		}
		terminer(ctx, f);
	}
}

/*
 * energise : indique si le mode énergie est actif pendant le k-ième coup simulé
 * (k = 1 pour le coup joué maintenant). Le moteur donne le nombre de tours
 * restants, le coup actuel compris : l'énergie dure donc tant que k <= remaining.
 */
static bool energise(const struct contexte *ctx, int k)
{
	return ctx->energy && k <= ctx->remaining;
}

/*
 * voit : indique si un fantôme en g voit Pacman (en pac) dans la direction d.
 * Stratégie : comme le moteur, on avance en ligne droite depuis le fantôme
 * jusqu'à un mur ou au bord de la carte (sans le traverser), et l'on regarde
 * si l'on rencontre Pacman.
 */
static bool voit(const struct contexte *ctx, int g, direction d, int pac)
{
	int x = g % ctx->xsize, y = g / ctx->xsize;

	while (x >= 0 && y >= 0 && x < ctx->xsize && y < ctx->ysize && ctx->map[y][x] != WALL)
	{
		if (y * ctx->xsize + x == pac)
			return true;
		x += (d == EAST) - (d == WEST);
		y += (d == SOUTH) - (d == NORTH);
	}
	return false;
}

/*
 * choix_fantome : calcule les directions que peut prendre un fantôme dans
 * l'état s, et leurs probabilités (rangées dans dirs et probas ; le nombre de
 * choix est renvoyé, 0 si le fantôme est bloqué).
 * Stratégie : on applique les règles du moteur.
 *   - Directions permises : pas de mur ; pas sur Pacman en mode énergie.
 *   - Hors mode énergie, un fantôme qui voit Pacman fonce sur lui.
 *   - Fantôme errant (fantôme 4, ou tous en mode easy) : dans un couloir
 *     (2 issues), il continue tout droit ou prend le virage sans faire
 *     demi-tour ; ailleurs, il tire une direction permise au hasard (en mode
 *     énergie, il évite celle où il voit Pacman).
 *   - Fantôme qui vise (1 à 3 en mode original) : sa cible est Pacman (1),
 *     la case PORTEE_CIBLE devant lui (2) ou derrière lui (3). Il classe les
 *     directions selon la position de la cible (tables du moteur) ; en mode
 *     énergie, s'il est près de sa cible, il prend les directions opposées
 *     (fuite). Aux carrefours, il prend la première direction permise ; dans
 *     un couloir, il garde sa direction tant qu'il le peut.
 */
static int choix_fantome(const struct contexte *ctx, int numero, const struct etat *s, int pac,
						 direction pdir, bool ener, int dirs[4], double probas[4])
{
	bool peut[4], vu[4];
	int d, nb_issues = 0, k = 0;

	// Directions permises et directions où Pacman est en vue.
	for (d = NORTH; d <= WEST; d++)
	{
		int v = ctx->gadj[4 * s->cellule + d];
		peut[d] = v >= 0 && !(ener && v == pac);
		vu[d] = peut[d] && voit(ctx, s->cellule, d, pac);
		if (peut[d])
			nb_issues++;
	}
	if (nb_issues == 0)
		return 0;

	// Poursuite à vue (hors mode énergie) : direction imposée.
	if (!ener)
		for (d = NORTH; d <= WEST; d++)
			if (vu[d])
			{
				dirs[0] = d;
				probas[0] = 1.0;
				return 1;
			}

	if (!s->cible)
	{
		// Fantôme errant. Dans un couloir : tout droit, sinon le virage.
		if (nb_issues == 2)
		{
			if (s->dir >= 0 && peut[s->dir] && !(vu[s->dir] && ener))
			{
				dirs[0] = s->dir;
				probas[0] = 1.0;
				return 1;
			}
			for (d = NORTH; d <= WEST; d++)
				if (peut[d] && (s->dir < 0 || d != (s->dir + 2) % 4) && !(vu[d] && ener))
				{
					dirs[0] = d;
					probas[0] = 1.0;
					return 1;
				}
		}
		// Sinon, tirage uniforme parmi les directions acceptées.
		for (d = NORTH; d <= WEST; d++)
			if (peut[d] && (!(vu[d] && ener) || nb_issues == 1))
				dirs[k++] = d;
		for (d = 0; d < k; d++)
			probas[d] = 1.0 / k;
		return k;
	}
	else
	{
		// Fantôme qui vise une case : tables de préférence du moteur (une paire
		// de lignes par quadrant : axe vertical dominant, puis horizontal).
		int tables[8][4] = {
			{NORTH, EAST, WEST, SOUTH}, {EAST, NORTH, SOUTH, WEST},
			{SOUTH, EAST, WEST, NORTH}, {EAST, SOUTH, NORTH, WEST},
			{SOUTH, WEST, EAST, NORTH}, {WEST, SOUTH, NORTH, EAST},
			{NORTH, WEST, EAST, SOUTH}, {WEST, NORTH, SOUTH, EAST}};
		int tx = pac % ctx->xsize, ty = pac / ctx->xsize;
		int dx, dy, ligne;
		bool fuite;

		// Cible : Pacman, ou PORTEE_CIBLE cases devant/derrière lui (bornée à la carte).
		if (numero == 2 || numero == 3)
		{
			direction vers = (numero == 2) ? pdir : (pdir + 2) % 4;
			tx += PORTEE_CIBLE * ((vers == EAST) - (vers == WEST));
			ty += PORTEE_CIBLE * ((vers == SOUTH) - (vers == NORTH));
			tx = tx < 0 ? 0 : (tx >= ctx->xsize ? ctx->xsize - 1 : tx);
			ty = ty < 0 ? 0 : (ty >= ctx->ysize ? ctx->ysize - 1 : ty);
		}
		dx = s->cellule % ctx->xsize - tx;
		dy = s->cellule / ctx->xsize - ty;

		// Choix de la table selon le quadrant de la cible, puis selon l'axe dominant.
		if (dx <= 0 && dy > 0)
			ligne = 0;
		else if (dx < 0 && dy <= 0)
			ligne = 2;
		else if (dx >= 0 && dy < 0)
			ligne = 4;
		else
			ligne = 6;
		if (dx * dx >= dy * dy)
			ligne++;

		// Fuite en mode énergie quand la cible est proche.
		fuite = ener && dx * dx + dy * dy < ctx->rayon_fuite2;

		// Dans un couloir, sans fuite : il garde sa direction.
		if (s->dir >= 0 && peut[s->dir] && nb_issues <= 2 && !fuite)
			dirs[0] = s->dir;
		else
		{
			dirs[0] = -1;
			for (k = 0; k < 4 && dirs[0] < 0; k++)
			{
				d = fuite ? (tables[ligne][k] + 2) % 4 : tables[ligne][k];
				if (peut[d])
					dirs[0] = d;
			}
		}
		probas[0] = 1.0;
		return 1;
	}
}

/*
 * ajouter : ajoute la probabilité p à l'état (cellule, dir, cible) de f.
 * Stratégie : chaque état possible a une clé unique ; le tableau index donne
 * sa place dans f s'il y est déjà (on cumule alors les probabilités), sinon
 * on l'ajoute à la fin (s'il reste de la place ; sinon il est ignoré, ce qui
 * ne concerne que des possibilités très improbables).
 */
static void ajouter(struct contexte *ctx, struct fantome *f, int cellule, int dir, int cible, double p)
{
	int cle = (cellule * 5 + dir + 1) * 2 + cible;

	if (ctx->index[cle] >= 0)
		f->e[ctx->index[cle]].p += p;
	else if (f->nb < MAX_ETATS)
	{
		ctx->index[cle] = f->nb;
		f->e[f->nb].cellule = cellule;
		f->e[f->nb].dir = dir;
		f->e[f->nb].cible = cible;
		f->e[f->nb].p = p;
		f->nb++;
	}
}

/*
 * terminer : remet à -1 les cases du tableau index utilisées pour construire f,
 * afin qu'il soit prêt pour l'ensemble suivant.
 */
static void terminer(struct contexte *ctx, const struct fantome *f)
{
	for (int i = 0; i < f->nb; i++)
		ctx->index[(f->e[i].cellule * 5 + f->e[i].dir + 1) * 2 + f->e[i].cible] = -1;
}

/*
 * avancer : calcule dans dst les états possibles d'un fantôme après un
 * déplacement, à partir de ses états possibles src.
 * Stratégie : chaque état de src est remplacé par ses successeurs (voir
 * choix_fantome), avec la probabilité de l'état multipliée par celle du choix.
 * En mode énergie, les fantômes ne bougent qu'un tour sur deux, sans que l'on
 * sache lequel : chaque état reste aussi sur place avec une chance sur deux.
 */
static void avancer(struct contexte *ctx, const struct fantome *src, struct fantome *dst, int pac,
					direction pdir, bool ener)
{
	int dirs[4], i, j, k;
	double probas[4];
	double bouge = ener ? 0.5 : 1.0; // probabilité que le fantôme bouge à ce tour

	dst->numero = src->numero;
	dst->nb = 0;
	for (i = 0; i < src->nb; i++)
	{
		const struct etat *s = &src->e[i];
		if (ener)
			ajouter(ctx, dst, s->cellule, s->dir, s->cible, s->p * 0.5);
		k = choix_fantome(ctx, src->numero, s, pac, pdir, ener, dirs, probas);
		if (k == 0) // fantôme bloqué : il reste sur place
			ajouter(ctx, dst, s->cellule, s->dir, s->cible, s->p * bouge);
		for (j = 0; j < k; j++)
			ajouter(ctx, dst, ctx->gadj[4 * s->cellule + dirs[j]], dirs[j], s->cible,
					s->p * bouge * probas[j]);
	}
	terminer(ctx, dst);
}

/*
 * retirer : enlève de f les états situés sur la case cellule et renvoie leur
 * probabilité totale q. Si normaliser est vrai (Pacman a survécu à la
 * rencontre), les probabilités restantes sont divisées par 1 - q : on
 * raisonne désormais sachant que le fantôme n'était pas sur cette case.
 */
static double retirer(struct fantome *f, int cellule, bool normaliser)
{
	double q = 0.0;
	int i, j = 0;

	// Compactage du tableau en sautant les états situés sur la case.
	for (i = 0; i < f->nb; i++)
	{
		if (f->e[i].cellule == cellule)
			q += f->e[i].p;
		else
			f->e[j++] = f->e[i];
	}
	f->nb = j;
	if (normaliser && q > 0.0 && q < 1.0)
		for (i = 0; i < f->nb; i++)
			f->e[i].p /= (1.0 - q);
	return q;
}

/*
 * deja_mange : indique si la case c a déjà été visitée par Pacman dans la
 * suite de coups en cours de simulation (son point est alors déjà mangé).
 */
static bool deja_mange(const struct contexte *ctx, int c)
{
	for (int i = 0; i < ctx->nb_chemin; i++)
		if (ctx->chemin[i] == c)
			return true;
	return false;
}

/*
 * chercher : renvoie la meilleure valeur espérée atteignable depuis la
 * position simulée (Pacman en pac après t coups, fantômes dans niveaux[t],
 * probabilité survie d'être encore en vie, gain espéré déjà accumulé).
 * Stratégie : exploration en profondeur des coups de Pacman jusqu'à
 * PROFONDEUR ; on garde le maximum. Pour limiter l'explosion combinatoire, un
 * demi-tour n'est envisagé qu'au premier coup. À la racine, la direction du
 * meilleur coup est écrite dans *meilleure : celle qui s'y trouve déjà (le
 * coup précédent) est testée en premier et n'est remplacée que par un coup
 * strictement meilleur.
 */
static double chercher(struct contexte *ctx, int pac, int t, double survie, double gain, int arrivee,
					   direction *meilleure)
{
	double meilleur = 0.0, valeur;
	bool trouve = false;
	int prefere = (meilleure != NULL) ? (int)*meilleure : 0;

	// Horizon atteint, ou Pacman presque certainement mort : on évalue la position.
	if (t == PROFONDEUR || survie * 1000 < SEUIL_SURVIE)
		return gain + survie * evaluer_feuille(ctx, pac, t);

	for (int k = 0; k < 4; k++)
	{
		direction d = (prefere + k) % 4;

		// Coup interdit (mur, porte) ou demi-tour (sauf au premier coup) : ignoré.
		if (ctx->padj[4 * pac + d] < 0 || (arrivee >= 0 && (int)d == (arrivee + 2) % 4))
			continue;

		valeur = jouer_coup(ctx, pac, d, t, survie, gain);

		// Au premier coup, petit bonus aléatoire : sans mémoire, Pacman ne peut
		// pas savoir qu'il tourne en rond ; ce bruit (faible devant le moindre
		// risque de mort) finit par lui faire prendre un autre chemin.
		if (t == 0)
			valeur += rand() % BRUIT;

		if (t == 0 && DEBUG)
		{
			directionprinter(d);
			printf(": %.0f\n", valeur);
		}

		if (!trouve || valeur > meilleur)
		{
			trouve = true;
			meilleur = valeur;
			if (meilleure != NULL)
				*meilleure = d;
		}
	}

	// Cul-de-sac (seul le demi-tour était possible) : on évalue sur place.
	if (!trouve)
		return gain + survie * evaluer_feuille(ctx, pac, t);
	return meilleur;
}

/*
 * jouer_coup : simule le coup d de Pacman (le (t+1)-ième), puis la réponse des
 * fantômes, et renvoie la meilleure valeur espérée atteignable ensuite.
 * Stratégie : on reproduit l'ordre du moteur :
 *   1. Pacman avance et mange le point ou l'énergie de la case ;
 *   2. pour chaque fantôme, la probabilité q qu'il soit sur cette case : en
 *      mode énergie, Pacman le mange (gain espéré) ; sinon Pacman meurt avec
 *      cette probabilité ;
 *   3. les fantômes avancent ; hors mode énergie, la probabilité r que l'un
 *      d'eux arrive sur Pacman est aussi une probabilité de mourir ;
 *   4. le gain espéré est mis à jour (gains et pénalité de mort pondérés par
 *      la probabilité d'être en vie), puis on poursuit la recherche.
 */
static double jouer_coup(struct contexte *ctx, int pac, direction d, int t, double survie, double gain)
{
	int np = ctx->padj[4 * pac + d]; // nouvelle position de Pacman
	int k = t + 1;					 // numéro du coup
	bool ener = energise(ctx, k);	 // mode énergie pendant ce coup ?
	struct groupe *avant = &ctx->niveaux[t], *apres = &ctx->niveaux[k];
	double vivant = 1.0; // probabilité de survivre à ce coup
	double valeur;
	char c = contenu(ctx, np);
	int i;

	// 1. Ce que Pacman mange (si la case n'a pas déjà été visitée dans la simulation).
	if (!deja_mange(ctx, np))
	{
		if (c == VIRGIN_PATH)
			gain += survie * (P_POINT - P_TOUR * k);
		else if (c == ENERGY)
		{
			// Une énergie est précieuse quand un fantôme menace (refuge et
			// chasse), inutile sinon, sauf s'il ne reste plus qu'elles à manger.
			if (ctx->menace || !ctx->reste_points)
				gain += survie * (P_ENERGIE_REFUGE - P_TOUR * k);
			else
				gain += survie * P_ENERGIE_RESERVE;
		}
	}

	// 2 et 3. Rencontres avec chaque fantôme, avant puis après son déplacement.
	apres->nb = avant->nb;
	for (i = 0; i < avant->nb; i++)
	{
		double q, r = 0.0;
		*ctx->tampon = avant->f[i];
		q = retirer(ctx->tampon, np, !ener);
		avancer(ctx, ctx->tampon, &apres->f[i], np, d, ener);
		if (ener)
			gain += survie * q * (P_FANTOME - P_TOUR * k); // fantôme mangé : aucun risque
		else
		{
			r = retirer(&apres->f[i], np, true);
			vivant *= (1.0 - q) * (1.0 - r);
		}
	}

	// 4. Pénalité de mort et suite de la recherche.
	gain += survie * (1.0 - vivant) * ((double)P_MORT + (double)P_MORT_TOUR * k);
	ctx->chemin[ctx->nb_chemin++] = np;
	valeur = chercher(ctx, np, k, survie * vivant, gain, d, NULL);
	ctx->nb_chemin--;
	return valeur;
}

/*
 * distances_fantomes : remplit dfant avec, pour chaque case, le nombre de pas
 * minimal pour qu'un fantôme de g l'atteigne.
 * Stratégie : BFS multi-sources partant de toutes les positions possibles des
 * fantômes dont la probabilité dépasse SEUIL_PRESENCE.
 */
static void distances_fantomes(struct contexte *ctx, const struct groupe *g)
{
	int debut = 0, fin = 0, i, j;

	for (i = 0; i < ctx->n; i++)
		ctx->dfant[i] = INFINI;
	for (i = 0; i < g->nb; i++)
		for (j = 0; j < g->f[i].nb; j++)
		{
			int c = g->f[i].e[j].cellule;
			if (g->f[i].e[j].p * 1000 >= SEUIL_PRESENCE && ctx->dfant[c] != 0)
			{
				ctx->dfant[c] = 0;
				ctx->file[fin++] = c;
			}
		}
	while (debut < fin)
	{
		int c = ctx->file[debut++];
		for (int d = 0; d < 4; d++)
		{
			int v = ctx->gadj[4 * c + d];
			if (v >= 0 && ctx->dfant[v] == INFINI)
			{
				ctx->dfant[v] = ctx->dfant[c] + 1;
				ctx->file[fin++] = v;
			}
		}
	}
}

/*
 * evaluer_feuille : note la position de Pacman (en pac, après t coups).
 * Stratégie : BFS depuis Pacman qui ne traverse que les cases « sûres »,
 * c'est-à-dire atteintes par Pacman avant tout fantôme dangereux (ou pendant
 * le mode énergie). On y mesure :
 *   - l'espace libre (nombre de cases sûres, plafonné à ESPACE_MAX) : un petit
 *     espace signifie que Pacman est pris au piège ;
 *   - la distance à la cible la plus intéressante : point, énergie (avec un
 *     détour si elle n'est pas nécessaire), fantôme mangeable.
 * Si aucune cible n'est accessible sans risque, une seconde BFS, sans la
 * contrainte de sûreté, donne la distance à la plus proche, pénalisée de
 * CIBLE_RISQUEE : Pacman se rapproche ainsi des derniers points au lieu
 * d'attendre sur place.
 * La note vaut - P_ESPACE x (espace manquant) - P_CIBLE x (distance à la cible).
 */
static double evaluer_feuille(struct contexte *ctx, int pac, int t)
{
	const struct groupe *g = &ctx->niveaux[t];
	int debut = 0, fin = 0, espace = 0, cible = CIBLE_MAX;
	int i, j;

	distances_fantomes(ctx, g);

	for (i = 0; i < ctx->n; i++)
		ctx->dist[i] = INFINI;
	ctx->dist[pac] = 0;
	ctx->file[fin++] = pac;
	while (debut < fin)
	{
		int c = ctx->file[debut++];
		int dc = ctx->dist[c];
		char car = contenu(ctx, c);

		if (espace < ESPACE_MAX)
			espace++;

		// Cible éventuelle (sauf si elle a déjà été mangée dans la simulation).
		if (dc > 0 && !deja_mange(ctx, c))
		{
			if (car == VIRGIN_PATH && dc < cible)
				cible = dc;
			else if (car == ENERGY)
			{
				int dd = dc + ((ctx->menace || !ctx->reste_points) ? 0 : DETOUR_ENERGIE);
				if (dd < cible)
					cible = dd;
			}
		}
		// Fantôme mangeable probablement présent ici : cible prioritaire.
		if (dc > 0 && energise(ctx, t + dc))
			for (i = 0; i < g->nb; i++)
				for (j = 0; j < g->f[i].nb; j++)
					if (g->f[i].e[j].cellule == c && g->f[i].e[j].p * 1000 >= 10 * SEUIL_PRESENCE &&
						dc - BONUS_CHASSE < cible)
						cible = dc - BONUS_CHASSE;

		// Arrêt anticipé : espace plafonné et plus aucune cible plus proche possible.
		if (espace >= ESPACE_MAX && dc - BONUS_CHASSE >= cible)
			break;

		// Voisines praticables et sûres : Pacman y arrive au coup t + dc + 1 ; un
		// fantôme dangereux ne doit pas pouvoir y être à ce moment-là.
		for (int d = 0; d < 4; d++)
		{
			int v = ctx->padj[4 * c + d];
			if (v >= 0 && ctx->dist[v] == INFINI &&
				(energise(ctx, t + dc + 1) || ctx->dfant[v] > dc + 1))
			{
				ctx->dist[v] = dc + 1;
				ctx->file[fin++] = v;
			}
		}
	}

	// Aucune cible sûre : distance réelle à la cible la plus proche, pénalisée.
	if (cible == CIBLE_MAX && ctx->nb_cibles > 0)
	{
		debut = fin = 0;
		for (i = 0; i < ctx->n; i++)
			ctx->dist[i] = INFINI;
		ctx->dist[pac] = 0;
		ctx->file[fin++] = pac;
		while (debut < fin && cible == CIBLE_MAX)
		{
			int c = ctx->file[debut++];
			char car = contenu(ctx, c);
			if ((car == VIRGIN_PATH || car == ENERGY) && !deja_mange(ctx, c) && c != pac)
				cible = CIBLE_RISQUEE + ctx->dist[c];
			for (int d = 0; d < 4; d++)
			{
				int v = ctx->padj[4 * c + d];
				if (v >= 0 && ctx->dist[v] == INFINI)
				{
					ctx->dist[v] = ctx->dist[c] + 1;
					ctx->file[fin++] = v;
				}
			}
		}
	}

	return -(double)P_ESPACE * (ESPACE_MAX - espace) - (double)P_CIBLE * cible;
}

/*
 * directionprinter : affiche le nom d'une direction (messages de debug).
 */
static void directionprinter(direction d)
{
	switch (d)
	{
	case NORTH: printf("NORTH"); break;
	case EAST:  printf("EAST");  break;
	case SOUTH: printf("SOUTH"); break;
	case WEST:  printf("WEST");  break;
	}
}
