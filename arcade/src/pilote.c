/*
 * pilote.c - Pilote automatique simple, pour le mode démo et les tests.
 *
 * Stratégie : une BFS multi-sources depuis les fantômes dangereux donne, pour
 * chaque case, le temps qu'il leur faut pour l'atteindre. Une seconde BFS
 * depuis Pac-Man ne traverse que les cases qu'il atteint nettement avant eux,
 * et s'arrête sur la première cible : fantôme effrayé à portée, fruit, point
 * ou énergie. Si aucune cible n'est accessible sans risque, Pac-Man s'éloigne
 * au mieux du fantôme dangereux le plus proche.
 */
#include "pilote.h"

enum { N = LARGEUR * HAUTEUR, INF = 1 << 20 };

/*
 * dangereux : le fantôme peut-il attraper Pac-Man ? (un fantôme effrayé le
 * redevient si l'effroi touche à sa fin)
 */
static bool dangereux(const jeu_t *j, const fantome_t *f)
{
	if (f->etat == F_MANGE || f->etat == F_RENTREE)
		return false;
	return !f->effraye || j->temps_effroi < 1.5;
}

void pilote_choisir(jeu_t *j)
{
	static int dfant[N], dist[N], file[N];
	static direction_t premier[N];
	int debut = 0, fin = 0;
	int depart = j->pac.y * LARGEUR + j->pac.x;
	direction_t meilleure = AUCUNE;
	int meilleure_marge = -INF;

	// 1. Distances aux fantômes dangereux (les fantômes traversent la porte).
	for (int i = 0; i < N; i++)
		dfant[i] = INF;
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		const fantome_t *f = &j->f[i];
		int c = f->y * LARGEUR + f->x;
		if (dangereux(j, f) && dfant[c] != 0)
		{
			dfant[c] = 0;
			file[fin++] = c;
		}
	}
	while (debut < fin)
	{
		int c = file[debut++];
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(c % LARGEUR + DX[d]), ny = c / LARGEUR + DY[d], v;
			case_t k = lab_case(&j->lab, nx, ny);
			if (k == C_MUR || k == C_NEANT)
				continue;
			v = ny * LARGEUR + nx;
			if (dfant[v] == INF)
			{
				dfant[v] = dfant[c] + 1;
				file[fin++] = v;
			}
		}
	}

	// 2. BFS « sûre » depuis Pac-Man jusqu'à la première cible.
	for (int i = 0; i < N; i++)
		dist[i] = INF;
	debut = fin = 0;
	dist[depart] = 0;
	premier[depart] = AUCUNE;
	file[fin++] = depart;
	while (debut < fin)
	{
		int c = file[debut++], x = c % LARGEUR, y = c / LARGEUR;
		case_t k = j->lab.c[y][x];
		bool cible = false;

		if (c != depart)
		{
			cible = k == C_POINT || k == C_ENERGIE ||
					(j->fruit_visible && x == FRUIT_X && y == FRUIT_Y);
			for (int i = 0; i < NB_FANTOMES; i++)
				if (j->f[i].etat == F_ACTIF && j->f[i].effraye && j->temps_effroi > 1.5 + dist[c] * 0.15 &&
					j->f[i].x == x && j->f[i].y == y)
					cible = true;
		}
		if (cible)
		{
			j->pac.voulue = premier[c];
			return;
		}
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(x + DX[d]), ny = y + DY[d], v;
			if (!lab_libre(&j->lab, nx, ny))
				continue;
			v = ny * LARGEUR + nx;
			// Case sûre : Pac-Man (un peu plus rapide) y arrive avant le fantôme le plus proche.
			if (dist[v] == INF && dfant[v] > dist[c] + 2)
			{
				dist[v] = dist[c] + 1;
				premier[v] = (c == depart) ? d : premier[c];
				file[fin++] = v;
			}
		}
	}

	// 3. Aucune cible sûre : s'éloigner au mieux des fantômes.
	for (direction_t d = HAUT; d <= DROITE; d++)
	{
		int nx = lab_x(j->pac.x + DX[d]), ny = j->pac.y + DY[d];
		if (lab_libre(&j->lab, nx, ny) && dfant[ny * LARGEUR + nx] > meilleure_marge)
		{
			meilleure_marge = dfant[ny * LARGEUR + nx];
			meilleure = d;
		}
	}
	if (meilleure != AUCUNE)
		j->pac.voulue = meilleure;
}
