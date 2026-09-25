/*
 * jeu.c - Règles générales d'une partie : niveaux, calendrier des modes,
 * effroi, sortie des fantômes de la maison, score, collisions.
 */
#include "jeu.h"

#include <stdarg.h>
#include <stdio.h>

#include "fantomes.h"

const int DX[4] = {0, -1, 0, 1};
const int DY[4] = {-1, 0, 1, 0};

direction_t opposee(direction_t d)
{
	return d == AUCUNE ? AUCUNE : (direction_t)((d + 2) % 4);
}

/*
 * jeu_param : paramètres d'un niveau, d'après les tables de l'arcade.
 */
param_niveau_t jeu_param(int niveau)
{
	// Durée de l'effroi (secondes) pour les niveaux 1 à 18, puis 0.
	static const double EFFROI[18] = {6, 5, 4, 3, 2, 5, 2, 2, 1, 5, 2, 1, 1, 3, 1, 1, 0, 1};
	param_niveau_t p;

	if (niveau <= 1)
	{
		p.pac = 0.80; p.pac_effroi = 0.90;
		p.fant = 0.75; p.fant_effroi = 0.50; p.tunnel = 0.40;
		p.elroy1 = 0.80; p.elroy2 = 0.85;
	}
	else if (niveau <= 4)
	{
		p.pac = 0.90; p.pac_effroi = 0.95;
		p.fant = 0.85; p.fant_effroi = 0.55; p.tunnel = 0.45;
		p.elroy1 = 0.90; p.elroy2 = 0.95;
	}
	else if (niveau <= 20)
	{
		p.pac = 1.00; p.pac_effroi = 1.00;
		p.fant = 0.95; p.fant_effroi = 0.60; p.tunnel = 0.50;
		p.elroy1 = 1.00; p.elroy2 = 1.05;
	}
	else
	{
		p.pac = 0.90; p.pac_effroi = 0.90;
		p.fant = 0.95; p.fant_effroi = 0.60; p.tunnel = 0.50;
		p.elroy1 = 1.00; p.elroy2 = 1.05;
	}

	// Cruise Elroy 1 se déclenche quand il reste ce nombre de points ; Elroy 2 à la moitié.
	p.elroy1_points = niveau <= 1 ? 20 : niveau == 2 ? 30 : niveau <= 5 ? 40 : niveau <= 8 ? 50
					: niveau <= 11 ? 60 : niveau <= 14 ? 80 : niveau <= 18 ? 100 : 120;
	p.elroy2_points = p.elroy1_points / 2;

	p.effroi = niveau <= 18 ? EFFROI[niveau - 1 < 0 ? 0 : niveau - 1] : 0.0;

	// Calendrier dispersion / poursuite (étapes 1 à 7 ; la 8e, poursuite, est sans fin).
	{
		static const double N1[7] = {7, 20, 7, 20, 5, 20, 5};
		static const double N2[7] = {7, 20, 7, 20, 5, 1033, 1.0 / 60};
		static const double N5[7] = {5, 20, 5, 20, 5, 1037, 1.0 / 60};
		const double *c = niveau <= 1 ? N1 : niveau <= 4 ? N2 : N5;
		for (int i = 0; i < 7; i++)
			p.calendrier[i] = c[i];
	}

	// Compteurs personnels : nombre de points à manger avant que chaque fantôme sorte.
	p.limite_points[BLINKY] = 0;
	p.limite_points[PINKY] = 0;
	p.limite_points[INKY] = niveau <= 1 ? 30 : 0;
	p.limite_points[CLYDE] = niveau <= 1 ? 60 : niveau == 2 ? 50 : 0;
	p.limite_temps = niveau <= 4 ? 4.0 : 3.0;

	// Fruits.
	if (niveau <= 1)       { p.fruit_points = 100;  p.fruit_nom = "Cerise"; }
	else if (niveau == 2)  { p.fruit_points = 300;  p.fruit_nom = "Fraise"; }
	else if (niveau <= 4)  { p.fruit_points = 500;  p.fruit_nom = "Orange"; }
	else if (niveau <= 6)  { p.fruit_points = 700;  p.fruit_nom = "Pomme"; }
	else if (niveau <= 8)  { p.fruit_points = 1000; p.fruit_nom = "Melon"; }
	else if (niveau <= 10) { p.fruit_points = 2000; p.fruit_nom = "Galaxian"; }
	else if (niveau <= 12) { p.fruit_points = 3000; p.fruit_nom = "Cloche"; }
	else                   { p.fruit_points = 5000; p.fruit_nom = "Clé"; }
	return p;
}

/*
 * jeu_journal : ajoute un message au journal des événements (les plus récents
 * sont affichés sous le labyrinthe).
 */
void jeu_journal(jeu_t *j, const char *format, ...)
{
	char texte[JOURNAL_LONGUEUR - 12];
	va_list args;

	va_start(args, format);
	vsnprintf(texte, sizeof texte, format, args);
	va_end(args);

	if (j->nb_journal == JOURNAL_TAILLE)
	{
		for (int i = 1; i < JOURNAL_TAILLE; i++)
			snprintf(j->journal[i - 1], JOURNAL_LONGUEUR, "%s", j->journal[i]);
		j->nb_journal--;
	}
	snprintf(j->journal[j->nb_journal++], JOURNAL_LONGUEUR, "[%6.1f s] %s", j->temps_total, texte);
	if (j->journal_console)
		printf("    %s\n", j->journal[j->nb_journal - 1]);
}

/*
 * jeu_alea : générateur pseudo-aléatoire (xorshift64*).
 */
unsigned jeu_alea(jeu_t *j)
{
	j->alea ^= j->alea >> 12;
	j->alea ^= j->alea << 25;
	j->alea ^= j->alea >> 27;
	return (unsigned)((j->alea * 2685821657736338717ULL) >> 32);
}

/*
 * jeu_elroy : niveau de « Cruise Elroy » de Blinky (0, 1 ou 2).
 */
int jeu_elroy(const jeu_t *j)
{
	param_niveau_t p = jeu_param(j->niveau);
	if (j->elroy_suspendu)
		return 0;
	if (j->lab.points_restants <= p.elroy2_points)
		return 2;
	if (j->lab.points_restants <= p.elroy1_points)
		return 1;
	return 0;
}

/*
 * placer_personnages : remet Pac-Man et les fantômes au départ, avec le
 * calendrier des modes et l'effroi réinitialisés.
 */
static void placer_personnages(jeu_t *j)
{
	j->pac.x = PACMAN_X;
	j->pac.y = PACMAN_Y;
	j->pac.dir = GAUCHE;
	j->pac.voulue = GAUCHE;
	j->pac.avance = 0.0;
	j->pac.pause = 0;

	j->mode = DISPERSION;
	j->etape = 0;
	j->temps_etape = 0.0;
	j->temps_effroi = 0.0;
	j->combo = 0;
	j->temps_sans_point = 0.0;
	j->fruit_visible = false;
	fantomes_placer(j);

	j->phase = P_PRET;
	j->chrono = 2.0;
}

/*
 * nouveau_niveau : labyrinthe plein, compteurs de sortie remis à zéro.
 */
static void nouveau_niveau(jeu_t *j)
{
	lab_init(&j->lab);
	j->manges_niveau = 0;
	j->compteur_global_actif = false;
	j->compteur_global = 0;
	j->elroy_suspendu = false;
	placer_personnages(j);
	for (int i = 0; i < NB_FANTOMES; i++)
		j->f[i].compteur_points = 0;
	jeu_journal(j, "Niveau %d", j->niveau);
}

/*
 * jeu_init : nouvelle partie (le record est conservé par l'appelant).
 */
void jeu_init(jeu_t *j, int niveau, uint64_t graine, double vitesse)
{
	j->niveau = niveau < 1 ? 1 : niveau;
	j->score = 0;
	j->vies = 3;
	j->vie_bonus = false;
	j->alea = graine ? graine : 0x9E3779B97F4A7C15ULL;
	j->vitesse = vitesse;
	j->nb_journal = 0;
	j->afficher_cibles = false;
	j->popup_temps = 0.0;
	j->temps_total = 0.0;
	j->stat_fantomes_manges = 0;
	j->stat_sorties_apres_effroi = 0;
	j->stat_changements_mode = 0;
	j->stat_vies_perdues = 0;
	nouveau_niveau(j);
	j->chrono = 3.0;
}

/*
 * ajouter_score : ajoute des points, donne la vie supplémentaire à 10 000.
 */
static void ajouter_score(jeu_t *j, int points)
{
	j->score += points;
	if (j->score > j->record)
		j->record = j->score;
	if (!j->vie_bonus && j->score >= 10000)
	{
		j->vie_bonus = true;
		j->vies++;
		jeu_journal(j, "Vie supplémentaire !");
	}
}

/*
 * popup : affiche brièvement des points gagnés sur le labyrinthe.
 */
static void popup(jeu_t *j, int x, int y, int points)
{
	j->popup_x = x;
	j->popup_y = y;
	j->popup_points = points;
	j->popup_temps = 1.0;
}

/*
 * changer_mode : passage dispersion <-> poursuite. Les fantômes actifs font
 * demi-tour (c'est le signe visible du changement dans l'arcade) et adoptent
 * le nouveau mode ; ceux qui sont dans la maison l'adopteront en sortant.
 */
static void changer_mode(jeu_t *j, mode_global_t mode)
{
	j->mode = mode;
	j->stat_changements_mode++;
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		if (f->etat == F_ACTIF)
		{
			f->mode = mode;
			f->demi_tour = true;
		}
	}
	jeu_journal(j, "Mode global : %s", mode == DISPERSION ? "DISPERSION" : "POURSUITE");
}

/*
 * debut_effroi : Pac-Man a mangé une énergie. Tous les fantômes (sauf les
 * yeux des fantômes mangés) deviennent effrayés, et ceux qui sont dans le
 * labyrinthe font demi-tour. Le calendrier dispersion/poursuite est suspendu.
 * Aux niveaux élevés, l'effroi dure 0 s : les fantômes font seulement demi-tour.
 */
static void debut_effroi(jeu_t *j)
{
	param_niveau_t p = jeu_param(j->niveau);

	j->combo = 0;
	j->temps_effroi = p.effroi;
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		if (f->etat == F_MANGE || f->etat == F_RENTREE)
			continue;
		if (f->etat == F_ACTIF)
			f->demi_tour = true;
		if (p.effroi > 0)
			f->effraye = true;
	}
	if (p.effroi > 0)
		jeu_journal(j, "Énergie : fantômes effrayés pendant %.0f s", p.effroi);
}

/*
 * fin_effroi : l'effroi est terminé, tous les fantômes redeviennent normaux.
 */
static void fin_effroi(jeu_t *j)
{
	j->temps_effroi = 0.0;
	for (int i = 0; i < NB_FANTOMES; i++)
		j->f[i].effraye = false;
	jeu_journal(j, "Fin de l'effroi, retour en %s", j->mode == DISPERSION ? "DISPERSION" : "POURSUITE");
}

/*
 * maj_modes : fait avancer l'effroi, sinon le calendrier dispersion/poursuite.
 */
static void maj_modes(jeu_t *j, double dt)
{
	param_niveau_t p = jeu_param(j->niveau);

	if (j->temps_effroi > 0)
	{
		j->temps_effroi -= dt;
		if (j->temps_effroi <= 0)
			fin_effroi(j);
		return;
	}
	if (j->etape < 7)
	{
		j->temps_etape += dt;
		if (j->temps_etape >= p.calendrier[j->etape])
		{
			j->temps_etape -= p.calendrier[j->etape];
			j->etape++;
			changer_mode(j, j->etape % 2 == 0 ? DISPERSION : POURSUITE);
		}
	}
}

/*
 * fantome_prefere : premier fantôme encore dans la maison, dans l'ordre de
 * sortie Pinky, Inky, Clyde (NULL s'il n'y en a pas).
 */
static fantome_t *fantome_prefere(jeu_t *j)
{
	for (int i = PINKY; i <= CLYDE; i++)
		if (j->f[i].etat == F_MAISON)
			return &j->f[i];
	return NULL;
}

/*
 * maj_sorties : libère les fantômes de la maison.
 *   - Compteurs personnels : le fantôme préféré sort quand son compteur de
 *     points atteint la limite du niveau (0 pour Pinky, qui sort tout de suite).
 *   - Minuterie : si Pac-Man ne mange rien pendant 4 s (3 s à partir du
 *     niveau 5), le fantôme préféré sort.
 */
static void maj_sorties(jeu_t *j, double dt)
{
	param_niveau_t p = jeu_param(j->niveau);
	fantome_t *f = fantome_prefere(j);

	j->temps_sans_point += dt;
	if (f == NULL)
		return;
	if (!j->compteur_global_actif && f->compteur_points >= p.limite_points[f->nom])
		fantome_liberer(j, f);
	else if (j->temps_sans_point >= p.limite_temps)
	{
		j->temps_sans_point = 0.0;
		fantome_liberer(j, f);
	}
}

/*
 * point_mange_sorties : met à jour les compteurs de sortie quand Pac-Man mange
 * un point ou une énergie. Après une vie perdue, le compteur global libère
 * Pinky à 7 points, Inky à 17 et Clyde à 32 (le compteur global est alors
 * abandonné au profit des compteurs personnels).
 */
static void point_mange_sorties(jeu_t *j)
{
	j->temps_sans_point = 0.0;
	if (j->compteur_global_actif)
	{
		j->compteur_global++;
		if (j->compteur_global == 7)
			fantome_liberer(j, &j->f[PINKY]);
		else if (j->compteur_global == 17)
			fantome_liberer(j, &j->f[INKY]);
		else if (j->compteur_global == 32 && j->f[CLYDE].etat == F_MAISON)
		{
			j->compteur_global_actif = false;
			j->compteur_global = 0;
			fantome_liberer(j, &j->f[CLYDE]);
		}
	}
	else
	{
		fantome_t *f = fantome_prefere(j);
		if (f != NULL)
			f->compteur_points++;
	}
}

/*
 * manger_case : Pac-Man mange ce qui se trouve sur sa case.
 */
static void manger_case(jeu_t *j)
{
	param_niveau_t p = jeu_param(j->niveau);
	case_t *c = &j->lab.c[j->pac.y][j->pac.x];

	if (*c == C_POINT || *c == C_ENERGIE)
	{
		bool energie = (*c == C_ENERGIE);
		*c = C_VIDE;
		j->lab.points_restants--;
		j->manges_niveau++;
		ajouter_score(j, energie ? 50 : 10);
		j->pac.pause = energie ? 3 : 1; // l'arcade fige Pac-Man 1 ou 3 images
		point_mange_sorties(j);
		if (energie)
			debut_effroi(j);
		if (j->manges_niveau == 70 || j->manges_niveau == 170)
		{
			j->fruit_visible = true;
			j->temps_fruit = 9.5;
		}
		if (j->lab.points_restants == 0)
		{
			j->phase = P_NIVEAU_FINI;
			j->chrono = 2.5;
			jeu_journal(j, "Niveau %d terminé !", j->niveau);
		}
	}
	else if (j->fruit_visible && j->pac.x == FRUIT_X && j->pac.y == FRUIT_Y)
	{
		j->fruit_visible = false;
		ajouter_score(j, p.fruit_points);
		popup(j, FRUIT_X, FRUIT_Y, p.fruit_points);
		jeu_journal(j, "%s mangée : %d points", p.fruit_nom, p.fruit_points);
	}
}

/*
 * jeu_verifier_collisions : Pac-Man et un fantôme sur la même case. Un
 * fantôme effrayé est mangé (200, 400, 800 puis 1 600 points par énergie) ;
 * un fantôme normal attrape Pac-Man ; les yeux sont inoffensifs.
 */
void jeu_verifier_collisions(jeu_t *j)
{
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		fantome_t *f = &j->f[i];
		int points;

		if (f->etat != F_ACTIF || f->x != j->pac.x || f->y != j->pac.y)
			continue;
		if (!f->effraye)
		{
			j->phase = P_MORT;
			j->chrono = 1.5;
			j->stat_vies_perdues++;
			jeu_journal(j, "Pac-Man attrapé par %s", fantome_nom(f->nom));
			return;
		}
		j->combo++;
		points = 200 << (j->combo > 4 ? 3 : j->combo - 1);
		ajouter_score(j, points);
		popup(j, f->x, f->y, points);
		f->etat = F_MANGE;
		f->effraye = false;
		f->demi_tour = false;
		j->stat_fantomes_manges++;
		j->phase = P_PAUSE_MANGE;
		j->chrono = 0.5;
		jeu_journal(j, "%s mangé : %d points", fantome_nom(f->nom), points);
	}
}

/*
 * avancer_pacman : Pac-Man avance case par case. En arrivant sur une case, il
 * prend la direction demandée si elle est libre (le joueur peut donc anticiper
 * un virage), sinon il continue tout droit, ou s'arrête contre un mur.
 */
static void avancer_pacman(jeu_t *j, double dt)
{
	param_niveau_t p = jeu_param(j->niveau);
	pacman_t *pac = &j->pac;

	if (pac->pause > 0)
	{
		pac->pause--;
		return;
	}
	pac->avance += (j->temps_effroi > 0 ? p.pac_effroi : p.pac) * VITESSE_MAX * j->vitesse * dt;
	while (pac->avance >= 1.0)
	{
		if (pac->voulue != AUCUNE && lab_libre(&j->lab, pac->x + DX[pac->voulue], pac->y + DY[pac->voulue]))
			pac->dir = pac->voulue;
		if (!lab_libre(&j->lab, pac->x + DX[pac->dir], pac->y + DY[pac->dir]))
		{
			pac->avance = 0.0;
			return;
		}
		pac->avance -= 1.0;
		pac->x = lab_x(pac->x + DX[pac->dir]);
		pac->y += DY[pac->dir];
		manger_case(j);
		jeu_verifier_collisions(j);
		if (j->phase != P_JEU || pac->pause > 0)
			return;
	}
}

/*
 * jeu_tic : fait avancer la partie de dt secondes.
 */
void jeu_tic(jeu_t *j, double dt)
{
	if (j->popup_temps > 0)
		j->popup_temps -= dt;

	switch (j->phase)
	{
	case P_PRET:
	case P_PAUSE_MANGE:
		j->chrono -= dt;
		if (j->chrono <= 0)
			j->phase = P_JEU;
		return;

	case P_MORT:
		j->chrono -= dt;
		if (j->chrono > 0)
			return;
		j->vies--;
		if (j->vies <= 0)
		{
			j->phase = P_GAME_OVER;
			jeu_journal(j, "GAME OVER - score %ld", j->score);
			return;
		}
		// Après une vie perdue : compteur global de sortie et Elroy suspendu.
		placer_personnages(j);
		j->compteur_global_actif = true;
		j->compteur_global = 0;
		j->elroy_suspendu = true;
		return;

	case P_NIVEAU_FINI:
		j->chrono -= dt;
		if (j->chrono <= 0)
		{
			j->niveau++;
			nouveau_niveau(j);
		}
		return;

	case P_GAME_OVER:
		return;

	case P_JEU:
		break;
	}

	j->temps_total += dt;
	maj_modes(j, dt);
	maj_sorties(j, dt);
	if (j->fruit_visible)
	{
		j->temps_fruit -= dt;
		if (j->temps_fruit <= 0)
			j->fruit_visible = false;
	}
	avancer_pacman(j, dt);
	if (j->phase == P_JEU)
		fantomes_avancer(j, dt);
}
