/*
 * fantomes.c - Intelligence des quatre fantômes, fidèle à l'arcade (1980).
 *
 * Principe commun : un fantôme ne décide de sa direction qu'en arrivant sur
 * une case. Il ne fait jamais demi-tour de lui-même ; parmi les autres
 * directions praticables, il prend celle qui le rapproche le plus (distance à
 * vol d'oiseau) de sa « case cible ». En cas d'égalité, l'ordre de priorité est
 * haut, gauche, bas, droite. Toute la personnalité d'un fantôme tient donc dans
 * le choix de sa case cible :
 *
 *   - DISPERSION : chacun vise un coin fixe, hors du labyrinthe, et finit par
 *     tourner autour du pâté de murs le plus proche ;
 *   - POURSUITE :
 *       Blinky vise la case de Pac-Man ;
 *       Pinky vise 4 cases devant Pac-Man (4 en haut ET 4 à gauche quand
 *         Pac-Man regarde vers le haut : bogue de débordement de l'arcade) ;
 *       Inky prend la case 2 devant Pac-Man (même bogue vers le haut) et
 *         double le vecteur qui va de Blinky à cette case ;
 *       Clyde vise Pac-Man s'il en est à plus de 8 cases, sinon son coin ;
 *   - EFFROI : pas de cible, direction tirée au hasard à chaque carrefour ;
 *   - MANGÉ : les yeux visent la porte de la maison, à grande vitesse.
 *
 * Blinky devient « Cruise Elroy » quand il reste peu de points : il accélère
 * et poursuit Pac-Man même pendant la dispersion.
 */
#include "fantomes.h"

#include <limits.h>

// Coins visés en dispersion (hors du labyrinthe, comme dans l'arcade).
static const int COIN_X[NB_FANTOMES] = {25, 2, 27, 0};
static const int COIN_Y[NB_FANTOMES] = {-3, -3, 31, 31};

// Positions de départ.
static const int DEPART_X[NB_FANTOMES] = {SORTIE_X, MAISON_X, MAISON_X - 2, MAISON_X + 2};
static const int DEPART_Y[NB_FANTOMES] = {SORTIE_Y, MAISON_Y, MAISON_Y, MAISON_Y};

/*
 * fantome_nom : nom d'un fantôme.
 */
const char *fantome_nom(nom_fantome_t n)
{
	static const char *NOMS[NB_FANTOMES] = {"Blinky", "Pinky", "Inky", "Clyde"};
	return NOMS[n];
}

/*
 * fantomes_placer : met les fantômes à leur position de départ. Blinky
 * commence dehors ; les trois autres attendent dans la maison.
 */
void fantomes_placer(jeu_t *j)
{
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		f->nom = (nom_fantome_t)i;
		f->x = DEPART_X[i];
		f->y = DEPART_Y[i];
		f->avance = 0.0;
		f->effraye = false;
		f->demi_tour = false;
		f->mode = j->mode;
		f->cible_x = f->x;
		f->cible_y = f->y;
		if (i == BLINKY)
		{
			f->etat = F_ACTIF;
			f->dir = GAUCHE;
		}
		else
		{
			f->etat = F_MAISON;
			f->dir = (i == PINKY) ? BAS : HAUT;
		}
	}
}

/*
 * fantome_liberer : fait sortir un fantôme de la maison.
 */
void fantome_liberer(jeu_t *j, fantome_t *f)
{
	if (f->etat != F_MAISON)
		return;
	f->etat = F_SORTIE;
	jeu_journal(j, "%s quitte sa place dans la maison", fantome_nom(f->nom));
}

/*
 * fantome_vitesse : vitesse d'un fantôme en cases par seconde, selon son état
 * et l'endroit où il se trouve (tables de l'arcade).
 */
double fantome_vitesse(const jeu_t *j, const fantome_t *f)
{
	param_niveau_t p = jeu_param(j->niveau);
	double v;

	if (f->etat == F_MANGE || f->etat == F_RENTREE)
		v = 1.6; // les yeux rentrent très vite
	else if (f->etat == F_MAISON || f->etat == F_SORTIE)
		v = 0.5;
	else if (lab_tunnel(f->x, f->y))
		v = p.tunnel;
	else if (f->effraye)
		v = p.fant_effroi;
	else if (f->nom == BLINKY && jeu_elroy(j) == 2)
		v = p.elroy2;
	else if (f->nom == BLINKY && jeu_elroy(j) == 1)
		v = p.elroy1;
	else
		v = p.fant;
	return v * VITESSE_MAX * j->vitesse;
}

/*
 * fantome_calculer_cible : détermine la case visée par le fantôme (voir
 * l'en-tête du fichier). Les calculs se font sur la grille de l'arcade, sans
 * tenir compte des murs ni du tunnel, et la cible peut être hors du labyrinthe.
 */
void fantome_calculer_cible(const jeu_t *j, fantome_t *f)
{
	const pacman_t *p = &j->pac;
	mode_global_t mode = f->mode;
	int tx, ty;

	if (f->etat == F_MANGE)
	{
		f->cible_x = SORTIE_X;
		f->cible_y = SORTIE_Y;
		return;
	}

	// Cruise Elroy : Blinky ne se disperse plus.
	if (f->nom == BLINKY && jeu_elroy(j) > 0)
		mode = POURSUITE;

	if (mode == DISPERSION)
	{
		f->cible_x = COIN_X[f->nom];
		f->cible_y = COIN_Y[f->nom];
		return;
	}

	switch (f->nom)
	{
	case BLINKY: // le poursuivant : droit sur Pac-Man
		tx = p->x;
		ty = p->y;
		break;

	case PINKY: // l'embusqué : 4 cases devant Pac-Man
		tx = p->x + 4 * DX[p->dir];
		ty = p->y + 4 * DY[p->dir];
		if (p->dir == HAUT) // bogue de l'arcade : aussi 4 cases à gauche
			tx -= 4;
		break;

	case INKY: // l'imprévisible : symétrique de Blinky par rapport à 2 cases devant Pac-Man
	{
		int px = p->x + 2 * DX[p->dir], py = p->y + 2 * DY[p->dir];
		if (p->dir == HAUT) // même bogue que Pinky
			px -= 2;
		tx = 2 * px - j->f[BLINKY].x;
		ty = 2 * py - j->f[BLINKY].y;
		break;
	}

	default: // CLYDE, le simplet : poursuit de loin, fuit vers son coin de près
	{
		int dx = f->x - p->x, dy = f->y - p->y;
		if (dx * dx + dy * dy > 64)
		{
			tx = p->x;
			ty = p->y;
		}
		else
		{
			tx = COIN_X[CLYDE];
			ty = COIN_Y[CLYDE];
		}
		break;
	}
	}
	f->cible_x = tx;
	f->cible_y = ty;
}

/*
 * choisir_direction : direction prise par un fantôme actif ou mangé en
 * arrivant sur sa case.
 *   - Demi-tour imposé (changement de mode, début d'effroi) : il est prioritaire.
 *   - Effroi : direction tirée au hasard ; si elle est impossible, on essaie
 *     les suivantes dans l'ordre haut, gauche, bas, droite.
 *   - Sinon : la direction (hors demi-tour) qui rapproche le plus de la cible,
 *     en interdisant de monter depuis les « zones rouges ».
 * Si aucune direction n'est possible (cul-de-sac), il fait demi-tour.
 */
static direction_t choisir_direction(jeu_t *j, fantome_t *f)
{
	direction_t retour = opposee(f->dir);
	direction_t meilleure = AUCUNE;
	long meilleure_dist = LONG_MAX;

	if (f->demi_tour)
	{
		f->demi_tour = false;
		if (lab_libre(&j->lab, f->x + DX[retour], f->y + DY[retour]))
			return retour;
	}

	if (f->effraye)
	{
		int r = (int)(jeu_alea(j) % 4);
		for (int k = 0; k < 4; k++)
		{
			direction_t d = (direction_t)((r + k) % 4);
			if (d != retour && lab_libre(&j->lab, f->x + DX[d], f->y + DY[d]))
				return d;
		}
		return retour;
	}

	fantome_calculer_cible(j, f);
	for (direction_t d = HAUT; d <= DROITE; d++)
	{
		int nx = f->x + DX[d], ny = f->y + DY[d];
		long dx, dy, dist;

		if (d == retour || !lab_libre(&j->lab, nx, ny))
			continue;
		if (d == HAUT && f->etat == F_ACTIF && lab_zone_rouge(f->x, f->y))
			continue;
		dx = nx - f->cible_x;
		dy = ny - f->cible_y;
		dist = dx * dx + dy * dy;
		if (dist < meilleure_dist) // strictement : l'ordre de la boucle départage
		{
			meilleure_dist = dist;
			meilleure = d;
		}
	}
	return meilleure == AUCUNE ? retour : meilleure;
}

/*
 * sortie_terminee : le fantôme vient de franchir la porte et entre dans le
 * labyrinthe.
 *
 * Correction de bogue : un fantôme qui sort de la maison n'est plus effrayé,
 * même si l'effroi dure encore (il était effrayé en attendant dans la maison,
 * ou il revient d'avoir été mangé). Il reprend alors le mode global en cours,
 * dispersion ou poursuite, au lieu de garder un ancien mode.
 */
static void sortie_terminee(jeu_t *j, fantome_t *f)
{
	bool etait_effraye = f->effraye;

	f->etat = F_ACTIF;
	f->dir = GAUCHE;
	f->demi_tour = false;
	f->effraye = false;
	f->mode = j->mode;

	if (etait_effraye)
	{
		j->stat_sorties_apres_effroi++;
		jeu_journal(j, "%s sort de la maison : n'est plus effrayé, passe en %s", fantome_nom(f->nom),
					f->mode == DISPERSION ? "DISPERSION" : "POURSUITE");
	}
	else
		jeu_journal(j, "%s sort de la maison en %s", fantome_nom(f->nom),
					f->mode == DISPERSION ? "DISPERSION" : "POURSUITE");

	// Après une vie perdue, Blinky ne redevient Elroy qu'une fois Clyde sorti.
	if (f->nom == CLYDE)
		j->elroy_suspendu = false;
}

/*
 * pas_fantome : avance le fantôme d'une case selon son état.
 */
static void pas_fantome(jeu_t *j, fantome_t *f)
{
	switch (f->etat)
	{
	case F_MAISON: // va-et-vient vertical en attendant de sortir
		if (f->dir != HAUT && f->dir != BAS)
			f->dir = HAUT;
		if (f->dir == HAUT && f->y <= MAISON_HAUT)
			f->dir = BAS;
		else if (f->dir == BAS && f->y >= MAISON_BAS)
			f->dir = HAUT;
		f->y += DY[f->dir];
		break;

	case F_SORTIE: // rejoindre le centre de la maison, puis monter par la porte
		if (f->x != MAISON_X && f->y != MAISON_Y)
		{
			f->dir = f->y < MAISON_Y ? BAS : HAUT;
			f->y += DY[f->dir];
		}
		else if (f->x != MAISON_X)
		{
			f->dir = f->x < MAISON_X ? DROITE : GAUCHE;
			f->x += DX[f->dir];
		}
		else
		{
			f->dir = HAUT;
			f->y--;
			if (f->y == SORTIE_Y)
				sortie_terminee(j, f);
		}
		break;

	case F_RENTREE: // les yeux descendent jusqu'au centre de la maison
		f->dir = BAS;
		f->y++;
		if (f->y >= MAISON_Y)
		{
			f->etat = F_SORTIE; // régénéré, il ressort aussitôt
			f->effraye = false;
			jeu_journal(j, "%s est régénéré dans la maison", fantome_nom(f->nom));
		}
		break;

	case F_MANGE:
		if (f->x == SORTIE_X && f->y == SORTIE_Y)
		{
			f->etat = F_RENTREE;
			pas_fantome(j, f);
			return;
		}
		/* fall through */
	case F_ACTIF:
		f->dir = choisir_direction(j, f);
		f->x = lab_x(f->x + DX[f->dir]);
		f->y += DY[f->dir];
		break;
	}
}

/*
 * fantomes_avancer : fait avancer chaque fantôme selon sa vitesse. Les
 * collisions avec Pac-Man sont vérifiées après chaque case parcourue.
 */
void fantomes_avancer(jeu_t *j, double dt)
{
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		f->avance += fantome_vitesse(j, f) * dt;
		while (f->avance >= 1.0)
		{
			f->avance -= 1.0;
			pas_fantome(j, f);
			jeu_verifier_collisions(j);
			if (j->phase != P_JEU)
				return;
		}
	}
}

/*
 * fantome_etat_texte : description de l'état d'un fantôme (affichage).
 */
const char *fantome_etat_texte(const jeu_t *j, const fantome_t *f)
{
	switch (f->etat)
	{
	case F_MAISON:
		return f->effraye ? "maison (effrayé)" : "maison";
	case F_SORTIE:
		return f->effraye ? "sort (effrayé)" : "sort de la maison";
	case F_MANGE:
	case F_RENTREE:
		return "MANGÉ (retour)";
	default:
		break;
	}
	if (f->effraye)
		return "EFFROI";
	if (f->nom == BLINKY && jeu_elroy(j) > 0)
		return jeu_elroy(j) == 2 ? "POURSUITE (Elroy 2)" : "POURSUITE (Elroy 1)";
	return f->mode == DISPERSION ? "DISPERSION" : "POURSUITE";
}
