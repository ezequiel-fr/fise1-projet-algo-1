/*
 * ia.c - Intelligence artificielle de Pac-Man : recherche arborescente sur
 * le vrai moteur de jeu.
 *
 * Contrairement à la version du projet (où Pac-Man ignore tout de l'état
 * interne des fantômes), l'IA dispose ici de l'état complet de la partie :
 * directions, modes, minuteries et même le générateur pseudo-aléatoire des
 * fantômes effrayés. Le jeu est donc entièrement déterministe pour elle :
 * en copiant la partie et en faisant tourner le moteur (jeu_tic), elle
 * connaît exactement l'avenir de chaque suite de coups.
 *
 * Stratégie :
 *   1. Un « coup » consiste à choisir une direction en arrivant sur une case,
 *      puis à faire tourner le moteur jusqu'à la case suivante.
 *   2. On explore en profondeur toutes les suites de coups jusqu'à HORIZON
 *      cases. Dans un couloir il n'y a qu'un coup possible (le demi-tour n'est
 *      envisagé qu'au premier coup, ou dans un cul-de-sac) : l'arbre ne se
 *      ramifie qu'aux carrefours, ce qui permet de voir loin.
 *   3. Chaque suite est notée :
 *        - mort : très mauvais (d'autant plus que c'est tôt) ;
 *        - niveau terminé : excellent ;
 *        - sinon, les points réellement gagnés (points, fantômes, fruit),
 *          moins une pénalité si la position finale est un piège (peu de
 *          cases atteignables avant les fantômes dangereux), moins un peu
 *          pour la distance au point le plus proche, plus un bonus pour s'approcher
 *          des fantômes effrayés encore mangeables, moins une pénalité pour
 *          une énergie gaspillée (mangée sans fantôme à proximité).
 *   4. On joue le premier coup de la meilleure suite.
 *
 * Garde-fous : un demi-tour coûte un peu (hystérésis), et une recherche à horizon limité peut « tourner en rond » (la
 * meilleure suite recule d'un pas à chaque décision). L'IA mémorise donc
 * depuis combien de décisions elle n'a rien mangé ; au-delà de SANS_GAIN_MAX,
 * elle est « pressée » : chaque point mangé compte beaucoup plus, et la
 * distance au point le plus proche devient déterminante (le danger restant
 * prioritaire), jusqu'au prochain point mangé. Si cela ne suffit pas
 * (SANS_GAIN_BLOQUE décisions), elle suit directement le plus court chemin
 * vers le point le plus proche, en n'utilisant la recherche que pour écarter
 * les coups mortels : chaque pas la rapproche d'un point, elle ne peut plus
 * tourner en rond.
 */
#include "ia.h"

#include <string.h>
#include <time.h>

enum
{
	HORIZON = 16,	// profondeur de la recherche, en cases parcourues
	TICS_MAX = 300, // garde-fou : tics simulés au plus pour un coup
	ESPACE_MAX = 30, // nombre de cases sûres au-delà duquel on ne se sent plus piégé
	SANS_GAIN_MAX = 30,	 // décisions sans rien manger avant d'être « pressé »
	SANS_GAIN_BLOQUE = 120 // ... avant de suivre directement le chemin le plus court
};

#define MORT (-1e9)
#define DT (1.0 / 60)

// Pile des copies de la partie (une par profondeur) : pas d'allocation pendant la recherche.
static jeu_t pile[HORIZON + 1];
static int menace_racine;		 // un fantôme dangereux est-il proche au début de la recherche ?
static long score_racine;
static int niveau_racine, energies_racine;
static bool presse_racine;		 // l'IA est-elle « pressée » (voir l'en-tête) ?
static int points_racine;		 // points restant à manger au début de la recherche

/*
 * dangereux : ce fantôme peut-il attraper Pac-Man dans un avenir proche ?
 */
static bool dangereux(const jeu_t *s, const fantome_t *f)
{
	if (f->etat == F_MANGE || f->etat == F_RENTREE || f->etat == F_MAISON)
		return false;
	return !f->effraye || s->temps_effroi < 1.0;
}

/*
 * terminal : la partie simulée est-elle finie pour cette branche ?
 */
static bool terminal(const jeu_t *s)
{
	return s->phase == P_MORT || s->phase == P_GAME_OVER || s->phase == P_NIVEAU_FINI ||
		   s->niveau != niveau_racine;
}

/*
 * jouer : Pac-Man prend la direction d et le moteur tourne jusqu'à ce qu'il
 * arrive sur la case suivante (ou jusqu'à la fin de la branche).
 */
static void jouer(jeu_t *s, direction_t d)
{
	int x = s->pac.x, y = s->pac.y;

	s->pac.voulue = d;
	for (int t = 0; t < TICS_MAX && !terminal(s); t++)
	{
		jeu_tic(s, DT);
		if (s->pac.x != x || s->pac.y != y)
			return;
	}
}

/*
 * distances_fantomes : BFS multi-sources depuis les fantômes dangereux (un
 * fantôme qui sort de la maison compte à partir de la case de sortie).
 */
static void distances_fantomes(const jeu_t *s, int dfant[HAUTEUR_MAX][LARGEUR_MAX])
{
	static int file[HAUTEUR_MAX * LARGEUR_MAX];
	const labyrinthe_t *l = &s->lab;
	int debut = 0, fin = 0;

	for (int y = 0; y < l->hauteur; y++)
		for (int x = 0; x < l->largeur; x++)
			dfant[y][x] = 1 << 20;
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		const fantome_t *f = &s->f[i];
		int fx = f->x, fy = f->y;
		if (f->etat == F_SORTIE && l->maison)
			fx = l->sortie_x, fy = l->sortie_y;
		else if (!dangereux(s, f))
			continue;
		if (dfant[fy][fx] != 0)
		{
			dfant[fy][fx] = 0;
			file[fin++] = fy * LARGEUR_MAX + fx;
		}
	}
	while (debut < fin)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX;
		debut++;
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(l, cx + DX[d]), ny = lab_y(l, cy + DY[d]);
			if (lab_libre(l, nx, ny) && dfant[ny][nx] > dfant[cy][cx] + 1)
			{
				dfant[ny][nx] = dfant[cy][cx] + 1;
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}
}

/*
 * evaluer : note d'une partie simulée après prof coups (voir l'en-tête).
 */
static double evaluer(const jeu_t *s, int prof)
{
	static int dfant[HAUTEUR_MAX][LARGEUR_MAX], dist[HAUTEUR_MAX][LARGEUR_MAX];
	static int file[HAUTEUR_MAX * LARGEUR_MAX];
	const labyrinthe_t *l = &s->lab;
	double note;
	int debut = 0, fin = 0, espace = 0, dpoint = -1, dproie = -1;

	if (s->phase == P_MORT || s->phase == P_GAME_OVER)
		return MORT + prof * 1e6;
	note = (double)(s->score - score_racine);
	if (s->phase == P_NIVEAU_FINI || s->niveau != niveau_racine)
		return note + 1e6 - prof;

	// Énergie gaspillée : mangée alors qu'aucun fantôme ne menaçait. La
	// pénalité reste faible (l'équivalent de 3 points) : plus forte, elle crée un « effet
	// d'horizon » (la meilleure suite s'arrête juste avant l'énergie, puis,
	// une case plus loin, ne peut plus l'éviter : l'IA fait demi-tour, et
	// ainsi de suite).
	if (!menace_racine)
		note -= 30.0 * (s->energies_mangees - energies_racine);

	// 1. BFS « sûre » depuis Pac-Man : cases atteintes avant les fantômes
	//    dangereux (espace libre) et fantôme effrayé encore mangeable le plus proche.
	distances_fantomes(s, dfant);
	for (int y = 0; y < l->hauteur; y++)
		for (int x = 0; x < l->largeur; x++)
			dist[y][x] = -1;
	dist[s->pac.y][s->pac.x] = 0;
	file[fin++] = s->pac.y * LARGEUR_MAX + s->pac.x;
	while (debut < fin)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX, dc = dist[cy][cx];
		debut++;
		if (espace < ESPACE_MAX)
			espace++;
		if (dproie < 0 && s->temps_effroi > 1.0 + dc * 0.15)
			for (int i = 0; i < NB_FANTOMES; i++)
				if (s->f[i].etat == F_ACTIF && s->f[i].effraye && s->f[i].x == cx && s->f[i].y == cy)
					dproie = dc;
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(l, cx + DX[d]), ny = lab_y(l, cy + DY[d]);
			if (lab_libre(l, nx, ny) && dist[ny][nx] < 0 && dfant[ny][nx] > dc + 1)
			{
				dist[ny][nx] = dc + 1;
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}

	// 2. BFS sans contrainte jusqu'au point (ou à l'énergie) le plus proche.
	debut = fin = 0;
	for (int y = 0; y < l->hauteur; y++)
		for (int x = 0; x < l->largeur; x++)
			dist[y][x] = -1;
	dist[s->pac.y][s->pac.x] = 0;
	file[fin++] = s->pac.y * LARGEUR_MAX + s->pac.x;
	while (debut < fin && dpoint < 0)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX, dc = dist[cy][cx];
		case_t c = (case_t)l->c[cy][cx];
		debut++;
		if (dc > 0 && (c == C_POINT || c == C_ENERGIE))
			dpoint = dc;
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(l, cx + DX[d]), ny = lab_y(l, cy + DY[d]);
			if (lab_libre(l, nx, ny) && dist[ny][nx] < 0)
			{
				dist[ny][nx] = dc + 1;
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}

	note -= 40.0 * (ESPACE_MAX - espace);
	// La distance au point le plus proche ne sert qu'à départager : même à
	// 60 cases, elle pèse moins qu'un point (10), sinon l'IA « garderait » les
	// points proches pour plus tard au lieu de les manger.
	// Pressée : chaque point mangé vaut 100 et chaque case de distance 1, si
	// bien que manger un point vaut toujours mieux que s'en approcher.
	if (presse_racine)
		note += 100.0 * (points_racine - l->points_restants) - (dpoint >= 0 ? dpoint : 0);
	else if (dpoint >= 0)
		note -= 0.15 * dpoint;
	if (dproie >= 0)
		note += 150.0 - 5.0 * dproie;
	return note;
}

/*
 * chercher : meilleure note atteignable depuis pile[prof] (Pac-Man vient
 * d'arriver sur une case en allant vers arrivee).
 */
static double chercher(int prof, direction_t arrivee)
{
	const jeu_t *s = &pile[prof];
	double meilleure = MORT * 10;
	bool demi_tour_seul = true;

	if (prof == HORIZON || terminal(s))
		return evaluer(s, prof);

	for (int passe = 0; passe < 2 && demi_tour_seul; passe++)
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			double note;
			if (!lab_libre(&s->lab, s->pac.x + DX[d], s->pac.y + DY[d]))
				continue;
			// Premier passage : pas de demi-tour ; le second (cul-de-sac) l'autorise.
			if ((d == opposee(arrivee)) != (passe == 1))
				continue;
			demi_tour_seul = false;
			pile[prof + 1] = *s;
			jouer(&pile[prof + 1], d);
			note = chercher(prof + 1, d);
			if (note > meilleure)
				meilleure = note;
		}
	return meilleure;
}

/*
 * ia_choisir : meilleure direction pour Pac-Man dans la partie j.
 */
/*
 * distance_points : distance (BFS multi-sources depuis tous les points et
 * énergies) de la case (x, y) au point le plus proche.
 */
static int distance_points(const jeu_t *j, int x, int y)
{
	static int dist[HAUTEUR_MAX][LARGEUR_MAX], file[HAUTEUR_MAX * LARGEUR_MAX];
	const labyrinthe_t *l = &j->lab;
	int debut = 0, fin = 0;

	for (int yy = 0; yy < l->hauteur; yy++)
		for (int xx = 0; xx < l->largeur; xx++)
		{
			dist[yy][xx] = -1;
			if (l->c[yy][xx] == C_POINT || l->c[yy][xx] == C_ENERGIE)
			{
				dist[yy][xx] = 0;
				file[fin++] = yy * LARGEUR_MAX + xx;
			}
		}
	while (debut < fin && dist[y][x] < 0)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX;
		debut++;
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(l, cx + DX[d]), ny = lab_y(l, cy + DY[d]);
			if (lab_libre(l, nx, ny) && dist[ny][nx] < 0)
			{
				dist[ny][nx] = dist[cy][cx] + 1;
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}
	return dist[y][x] < 0 ? 1 << 20 : dist[y][x];
}

direction_t ia_choisir(const jeu_t *j, int urgence)
{
	direction_t choix = j->pac.dir, ordre[4];
	double meilleure = MORT * 10;
	int n = 0, plus_court = 1 << 30;
	bool presse = urgence >= 1;

	pile[0] = *j;
	pile[0].silencieux = true;
	pile[0].journal_console = false;
	score_racine = j->score;
	niveau_racine = j->niveau;
	energies_racine = j->energies_mangees;
	presse_racine = presse;
	points_racine = j->lab.points_restants;
	{
		static int dfant[HAUTEUR_MAX][LARGEUR_MAX];
		distances_fantomes(j, dfant);
		menace_racine = dfant[j->pac.y][j->pac.x] <= 8;
	}

	// La direction actuelle est examinée en premier : elle est gardée en cas d'égalité.
	ordre[n++] = j->pac.dir;
	for (direction_t d = HAUT; d <= DROITE; d++)
		if (d != j->pac.dir)
			ordre[n++] = d;

	for (int k = 0; k < n; k++)
	{
		direction_t d = ordre[k];
		double note;
		if (d == AUCUNE || !lab_libre(&j->lab, j->pac.x + DX[d], j->pac.y + DY[d]))
			continue;
		pile[1] = pile[0];
		jouer(&pile[1], d);
		note = chercher(1, d);
		// Hystérésis : faire demi-tour coûte un peu (bien moins que le moindre
		// risque d'être attrapé), pour ne pas hésiter indéfiniment entre deux
		// plans presque équivalents dans des directions opposées.
		if (d == opposee(j->pac.dir))
			note -= presse ? 60.0 : 5.0;
		// Bloquée : le plus court chemin vers un point, parmi les coups non mortels.
		if (urgence >= 2 && note > MORT / 2)
		{
			int dist = distance_points(j, lab_x(&j->lab, j->pac.x + DX[d]), lab_y(&j->lab, j->pac.y + DY[d]));
			if (dist < plus_court)
			{
				plus_court = dist;
				choix = d;
			}
			continue;
		}
		if (note > meilleure)
		{
			meilleure = note;
			choix = d;
		}
	}
	return choix;
}

void ia_init(ia_t *ia)
{
	memset(ia, 0, sizeof *ia);
}

/*
 * ia_piloter : à appeler à chaque tic. L'IA ne décide qu'en arrivant sur une
 * nouvelle case (ou quand la phase de jeu change) ; entre-temps, elle garde
 * sa direction, que le moteur applique au moment de quitter la case.
 */
void ia_piloter(ia_t *ia, jeu_t *j)
{
	if (!ia->valide || ia->x != j->pac.x || ia->y != j->pac.y || ia->phase != j->phase ||
		ia->niveau != j->niveau)
	{
		clock_t debut = clock();
		if (j->score != ia->score_precedent || j->phase != P_JEU)
			ia->sans_gain = 0;
		else
			ia->sans_gain++;
		ia->score_precedent = j->score;
		ia->choix = ia_choisir(j, ia->sans_gain > SANS_GAIN_BLOQUE ? 2 : ia->sans_gain > SANS_GAIN_MAX ? 1 : 0);
		ia->duree_totale += (double)(clock() - debut) / CLOCKS_PER_SEC;
		ia->decisions++;
		ia->x = j->pac.x;
		ia->y = j->pac.y;
		ia->phase = j->phase;
		ia->niveau = j->niveau;
		ia->valide = true;
	}
	j->pac.voulue = ia->choix;
}
