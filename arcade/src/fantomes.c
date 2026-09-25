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
 *   - MANGÉ : les yeux rentrent à la maison à grande vitesse, par le plus
 *     court chemin (voir choisir_direction).
 *
 * Blinky devient « Cruise Elroy » quand il reste peu de points : il accélère
 * et poursuit Pac-Man même pendant la dispersion.
 */
#include "fantomes.h"

#include <limits.h>

/*
 * coin : case visée en dispersion, hors du labyrinthe comme dans l'arcade :
 * en haut à droite (Blinky), en haut à gauche (Pinky), en bas à droite
 * (Inky), en bas à gauche (Clyde).
 */
static void coin(const labyrinthe_t *l, nom_fantome_t n, int *x, int *y)
{
	*x = (n == BLINKY) ? l->largeur - 3 : (n == PINKY) ? 2 : (n == INKY) ? l->largeur - 1 : 0;
	*y = (n == BLINKY || n == PINKY) ? -3 : l->hauteur;
}

/*
 * avancer_case : déplace (x, y) d'une case dans la direction d, en passant
 * par les bords du labyrinthe (tunnels).
 */
static void avancer_case(const labyrinthe_t *l, int *x, int *y, direction_t d)
{
	*x = lab_x(l, *x + DX[d]);
	*y = lab_y(l, *y + DY[d]);
}

/*
 * fantome_nom : nom d'un fantôme.
 */
const char *fantome_nom(nom_fantome_t n)
{
	static const char *NOMS[NB_FANTOMES] = {"Blinky", "Pinky", "Inky", "Clyde"};
	return NOMS[n];
}

/*
 * fantomes_placer : met les fantômes à leur position de départ, lue sur la
 * carte. Ceux qui commencent dans la maison y attendent leur tour (dans les
 * cartes fournies, Blinky commence dehors et les trois autres dedans).
 */
void fantomes_placer(jeu_t *j)
{
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		f->nom = (nom_fantome_t)i;
		f->x = j->lab.fant_x[i];
		f->y = j->lab.fant_y[i];
		f->avance = 0.0;
		f->effraye = false;
		f->demi_tour = false;
		f->mode = j->mode;
		f->cible_x = f->x;
		f->cible_y = f->y;
		if (lab_case(&j->lab, f->x, f->y) != C_MAISON)
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
	else if (lab_tunnel(&j->lab, f->x, f->y))
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

	// Les yeux visent la case devant la porte (ou, sans maison, leur départ).
	if (f->etat == F_MANGE)
	{
		f->cible_x = j->lab.maison ? j->lab.sortie_x : j->lab.fant_x[f->nom];
		f->cible_y = j->lab.maison ? j->lab.sortie_y : j->lab.fant_y[f->nom];
		return;
	}

	// Cruise Elroy : Blinky ne se disperse plus.
	if (f->nom == BLINKY && jeu_elroy(j) > 0)
		mode = POURSUITE;

	if (mode == DISPERSION)
	{
		coin(&j->lab, f->nom, &f->cible_x, &f->cible_y);
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
			coin(&j->lab, CLYDE, &tx, &ty);
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
		f->demi_tour = false;
	}

	// Yeux : plus court chemin jusqu'à la maison. (Avec la règle de l'arcade,
	// glouton et sans demi-tour, les yeux peuvent tourner en rond indéfiniment
	// dans certaines cartes de la version initiale.)
	if (f->etat == F_MANGE && j->lab.maison)
	{
		direction_t meilleure_d = retour;
		unsigned meilleure_l = 0xffff;
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(&j->lab, f->x + DX[d]), ny = lab_y(&j->lab, f->y + DY[d]);
			if (lab_libre(&j->lab, nx, ny) && j->lab.dist_sortie[ny][nx] < meilleure_l)
			{
				meilleure_l = j->lab.dist_sortie[ny][nx];
				meilleure_d = d;
			}
		}
		fantome_calculer_cible(j, f);
		return meilleure_d;
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
		if (d == HAUT && f->etat == F_ACTIF && lab_zone_rouge(&j->lab, f->x, f->y))
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
 * pas_maison : va-et-vient dans la maison en attendant de sortir (vertical si
 * la maison est assez haute, sinon horizontal, sinon sur place).
 */
static void pas_maison(jeu_t *j, fantome_t *f)
{
	const labyrinthe_t *l = &j->lab;
	direction_t d0 = f->dir == AUCUNE ? HAUT : f->dir;
	direction_t essais[6] = {d0, opposee(d0), HAUT, BAS, GAUCHE, DROITE};

	// On continue dans le même sens, sinon on repart en sens inverse, sinon
	// on essaie les autres directions.
	for (int k = 0; k < 6; k++)
		if (lab_case(l, f->x + DX[essais[k]], f->y + DY[essais[k]]) == C_MAISON)
		{
			f->dir = essais[k];
			avancer_case(l, &f->x, &f->y, f->dir);
			return;
		}
}

/*
 * pas_sortie : rejoindre le centre de la maison (en suivant les distances
 * précalculées), passer la porte, puis arriver devant : sortie terminée.
 */
static void pas_sortie(jeu_t *j, fantome_t *f)
{
	const labyrinthe_t *l = &j->lab;

	if (f->x == l->porte_x && f->y == l->porte_y)
	{
		f->dir = (direction_t)l->dir_sortie;
		f->x = l->sortie_x;
		f->y = l->sortie_y;
		sortie_terminee(j, f);
	}
	else if (f->x == l->centre_x && f->y == l->centre_y)
	{
		f->dir = (direction_t)l->dir_sortie;
		f->x = l->porte_x;
		f->y = l->porte_y;
	}
	else
	{
		for (direction_t d = HAUT; d <= DROITE; d++)
		{
			int nx = lab_x(l, f->x + DX[d]), ny = lab_y(l, f->y + DY[d]);
			if (l->c[ny][nx] == C_MAISON && l->dist_maison[ny][nx] < l->dist_maison[f->y][f->x])
			{
				f->dir = d;
				f->x = nx;
				f->y = ny;
				return;
			}
		}
		f->x = l->centre_x; // (ne devrait pas arriver) : retour direct au centre
		f->y = l->centre_y;
	}
}

/*
 * pas_fantome : avance le fantôme d'une case selon son état.
 */
static void pas_fantome(jeu_t *j, fantome_t *f)
{
	const labyrinthe_t *l = &j->lab;

	switch (f->etat)
	{
	case F_MAISON:
		pas_maison(j, f);
		break;

	case F_SORTIE:
		pas_sortie(j, f);
		break;

	case F_RENTREE: // les yeux passent la porte et se régénèrent derrière
		f->dir = opposee((direction_t)l->dir_sortie);
		if (f->x == l->porte_x && f->y == l->porte_y)
		{
			f->x = l->centre_x;
			f->y = l->centre_y;
			f->etat = F_SORTIE; // régénéré, il ressort aussitôt
			f->effraye = false;
			jeu_journal(j, "%s est régénéré dans la maison", fantome_nom(f->nom));
		}
		else
		{
			f->x = l->porte_x;
			f->y = l->porte_y;
		}
		break;

	case F_MANGE:
		fantome_calculer_cible(j, f);
		if (f->x == f->cible_x && f->y == f->cible_y)
		{
			if (l->maison)
			{
				f->etat = F_RENTREE;
				pas_fantome(j, f);
			}
			else
				sortie_terminee(j, f); // sans maison : régénéré sur place
			return;
		}
		/* fall through */
	case F_ACTIF:
		f->dir = choisir_direction(j, f);
		avancer_case(l, &f->x, &f->y, f->dir);
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
