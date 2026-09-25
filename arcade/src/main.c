/*
 * main.c - Boucle principale : jeu au clavier, démo, et tests automatiques.
 *
 *   ./pacman               jouer
 *   ./pacman --demo        regarder le pilote automatique jouer
 *   ./pacman --test [N]    N parties simulées sans affichage, avec vérifications
 *
 * Options : --niveau N, --vitesse F (1 = vitesse de l'arcade), --graine N.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fantomes.h"
#include "jeu.h"
#include "pilote.h"
#include "rendu.h"
#include "terminal.h"

enum { TICS_PAR_SECONDE = 60, IMAGES_PAR_SECONDE = 30 };

static double maintenant(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + t.tv_nsec / 1e9;
}

static void dormir(double secondes)
{
	struct timespec t;
	t.tv_sec = (time_t)secondes;
	t.tv_nsec = (long)((secondes - (double)t.tv_sec) * 1e9);
	nanosleep(&t, NULL);
}

static void aide(void)
{
	puts("Pac-Man (arcade) dans le terminal\n"
		 "\n"
		 "  ./pacman [--demo] [--niveau N] [--vitesse F] [--graine N]\n"
		 "  ./pacman --test [N] [--graine N] [--verbeux]\n"
		 "\n"
		 "  --demo       le pilote automatique joue (touche I pour l'activer ou le couper)\n"
		 "  --niveau N   niveau de départ (1 par défaut)\n"
		 "  --vitesse F  facteur de vitesse, 1 = vitesse de l'arcade (défaut 1)\n"
		 "  --graine N   graine du hasard (mouvements des fantômes effrayés)\n"
		 "  --test N     simule N parties sans affichage et vérifie les règles\n"
		 "  --verbeux    avec --test : affiche le journal des événements\n"
		 "\n"
		 "Touches : flèches, ZQSD ou WASD pour diriger ; P pause ; C cibles des\n"
		 "fantômes ; I pilote automatique (IA) ; N nouvelle partie ; X ou Échap pour quitter.");
}

/*
 * verifier_cibles : vérifie le calcul des cibles de chaque fantôme sur des
 * situations connues (règles de l'arcade). Renvoie le nombre d'erreurs.
 */
static int verifier_cibles(void)
{
	static jeu_t j;
	int erreurs = 0;

#define ATTENDU(n, ex, ey, description)                                                             \
	do                                                                                              \
	{                                                                                               \
		fantome_calculer_cible(&j, &j.f[n]);                                                        \
		if (j.f[n].cible_x != (ex) || j.f[n].cible_y != (ey))                                       \
		{                                                                                           \
			printf("  ERREUR %s : cible (%d,%d), attendu (%d,%d)\n", description, j.f[n].cible_x, \
				   j.f[n].cible_y, ex, ey);                                                         \
			erreurs++;                                                                              \
		}                                                                                           \
		else                                                                                        \
			printf("  ok     %s -> (%d,%d)\n", description, ex, ey);                              \
	} while (0)

	jeu_init(&j, 1, 1, 1.0);
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		j.f[i].etat = F_ACTIF;
		j.f[i].mode = POURSUITE;
	}
	j.pac.x = 13, j.pac.y = 23, j.pac.dir = GAUCHE;
	j.f[BLINKY].x = 20, j.f[BLINKY].y = 20;
	ATTENDU(BLINKY, 13, 23, "Blinky, poursuite : Pac-Man lui-même");
	ATTENDU(PINKY, 9, 23, "Pinky, Pac-Man vers la gauche : 4 cases devant");
	j.pac.dir = HAUT;
	ATTENDU(PINKY, 9, 19, "Pinky, Pac-Man vers le haut : 4 en haut et 4 à gauche (bogue)");
	j.pac.dir = DROITE;
	// Pivot 2 cases devant Pac-Man : (15,23) ; Blinky en (20,20) ; cible = 2*pivot - Blinky.
	ATTENDU(INKY, 10, 26, "Inky, Pac-Man vers la droite : double du vecteur Blinky -> pivot");
	j.f[CLYDE].x = 13, j.f[CLYDE].y = 11;
	ATTENDU(CLYDE, 13, 23, "Clyde loin de Pac-Man (12 cases) : poursuite");
	j.f[CLYDE].x = 13, j.f[CLYDE].y = 17;
	ATTENDU(CLYDE, 0, 31, "Clyde près de Pac-Man (6 cases) : son coin");
	for (int i = 0; i < NB_FANTOMES; i++)
		j.f[i].mode = DISPERSION;
	ATTENDU(BLINKY, 25, -3, "Blinky, dispersion : coin haut droit");
	ATTENDU(PINKY, 2, -3, "Pinky, dispersion : coin haut gauche");
	ATTENDU(INKY, 27, 31, "Inky, dispersion : coin bas droit");
	ATTENDU(CLYDE, 0, 31, "Clyde, dispersion : coin bas gauche");
	j.lab.points_restants = 15; // sous le seuil d'Elroy 1 (20) au niveau 1
	ATTENDU(BLINKY, 13, 23, "Blinky en Cruise Elroy : poursuit même en dispersion");
#undef ATTENDU
	return erreurs;
}

/*
 * verifier_sortie_maison : scénario de la correction demandée. Pinky attend
 * dans la maison, effrayée, avec un ancien mode (DISPERSION) alors que le mode
 * global est passé en POURSUITE. On la libère pendant l'effroi : en sortant,
 * elle ne doit plus être effrayée et doit suivre le mode global, alors que
 * l'effroi continue pour les fantômes restés dans le labyrinthe.
 */
static int verifier_sortie_maison(void)
{
	static jeu_t j;
	fantome_t *pinky = &j.f[PINKY];
	int tics = 0;

	jeu_init(&j, 1, 1, 1.0);
	j.phase = P_JEU;
	j.mode = POURSUITE;
	j.etape = 1;
	j.temps_effroi = 5.0;
	j.f[BLINKY].effraye = true;
	pinky->effraye = true;
	pinky->mode = DISPERSION;
	fantome_liberer(&j, pinky);
	while (pinky->etat != F_ACTIF && tics++ < 600)
		jeu_tic(&j, 1.0 / TICS_PAR_SECONDE);

	if (pinky->etat == F_ACTIF && !pinky->effraye && pinky->mode == POURSUITE && j.temps_effroi > 0 &&
		j.f[BLINKY].effraye)
	{
		printf("  ok     Pinky sort de la maison pendant l'effroi : plus effrayée, en POURSUITE "
			   "(Blinky, dehors, reste effrayé)\n");
		return 0;
	}
	printf("  ERREUR sortie de la maison : état %d, effrayée %d, mode %d, effroi %.1f s\n", pinky->etat,
		   pinky->effraye, pinky->mode, j.temps_effroi);
	return 1;
}

/*
 * tester : simule des parties avec le pilote automatique et vérifie à chaque
 * tic les règles importantes, dont la correction demandée : un fantôme qui
 * vient de sortir de la maison n'est jamais effrayé et suit le mode global.
 */
static int tester(int parties, uint64_t graine, bool verbeux)
{
	static jeu_t j;
	int erreurs = verifier_cibles() + verifier_sortie_maison();
	const double dt = 1.0 / TICS_PAR_SECONDE;

	for (int partie = 0; partie < parties; partie++)
	{
		etat_fantome_t avant[NB_FANTOMES];
		long tics = 0;

		// Les parties commencent aux niveaux 1 à 5, pour couvrir les différentes tables.
		jeu_init(&j, 1 + partie % 5, graine + (uint64_t)partie * 7919, 1.0);
		j.journal_console = verbeux;
		while (j.phase != P_GAME_OVER && tics < 20L * 60 * TICS_PAR_SECONDE && j.niveau <= 8)
		{
			for (int i = 0; i < NB_FANTOMES; i++)
				avant[i] = j.f[i].etat;
			pilote_choisir(&j);
			jeu_tic(&j, dt);
			tics++;

			if (!lab_libre(&j.lab, j.pac.x, j.pac.y) && lab_case(&j.lab, j.pac.x, j.pac.y) != C_VIDE)
			{
				printf("  ERREUR partie %d : Pac-Man dans un mur en (%d,%d)\n", partie, j.pac.x, j.pac.y);
				erreurs++;
			}
			for (int i = 0; i < NB_FANTOMES; i++)
			{
				const fantome_t *f = &j.f[i];
				case_t c = lab_case(&j.lab, f->x, f->y);
				if (avant[i] == F_SORTIE && f->etat == F_ACTIF && (f->effraye || f->mode != j.mode))
				{
					printf("  ERREUR partie %d : %s sort de la maison effrayé ou dans un ancien mode\n",
						   partie, fantome_nom(f->nom));
					erreurs++;
				}
				if (f->etat == F_ACTIF && f->effraye && j.temps_effroi <= 0)
				{
					printf("  ERREUR partie %d : %s effrayé hors effroi\n", partie, fantome_nom(f->nom));
					erreurs++;
				}
				if (c == C_MUR || c == C_NEANT || (f->etat == F_ACTIF && !lab_libre(&j.lab, f->x, f->y)))
				{
					printf("  ERREUR partie %d : %s sur une case interdite (%d,%d)\n", partie,
						   fantome_nom(f->nom), f->x, f->y);
					erreurs++;
				}
			}
			if (erreurs > 20)
				return erreurs;
		}
		printf("  partie %2d : niveau %d -> %d, score %6ld, %3d fantômes mangés, %3d sorties après effroi, "
			   "%3d changements de mode, %d vies perdues, %.0f s\n",
			   partie + 1, 1 + partie % 5, j.niveau, j.score, j.stat_fantomes_manges, j.stat_sorties_apres_effroi,
			   j.stat_changements_mode, j.stat_vies_perdues, j.temps_total);
	}
	return erreurs;
}

int main(int argc, char **argv)
{
	static jeu_t j;
	bool demo = false, pause = false, verbeux = false;
	int niveau = 1, parties = -1;
	double vitesse = 1.0, t_prec, reste = 0.0, t_image = 0.0;
	uint64_t graine = (uint64_t)time(NULL);

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--demo") == 0)
			demo = true;
		else if (strcmp(argv[i], "--verbeux") == 0)
			verbeux = true;
		else if (strcmp(argv[i], "--niveau") == 0 && i + 1 < argc)
			niveau = atoi(argv[++i]);
		else if (strcmp(argv[i], "--vitesse") == 0 && i + 1 < argc)
			vitesse = atof(argv[++i]);
		else if (strcmp(argv[i], "--graine") == 0 && i + 1 < argc)
			graine = strtoull(argv[++i], NULL, 10);
		else if (strcmp(argv[i], "--test") == 0)
			parties = (i + 1 < argc && argv[i + 1][0] != '-') ? atoi(argv[++i]) : 10;
		else
		{
			aide();
			return strcmp(argv[i], "--aide") == 0 || strcmp(argv[i], "-h") == 0 ? 0 : 1;
		}
	}
	if (vitesse <= 0)
		vitesse = 1.0;

	if (parties >= 0)
	{
		int erreurs;
		printf("Vérifications des règles des fantômes :\n");
		erreurs = tester(parties, graine, verbeux);
		printf("%s (%d erreur%s)\n", erreurs ? "ÉCHEC" : "SUCCÈS", erreurs, erreurs > 1 ? "s" : "");
		return erreurs ? 1 : 0;
	}

	jeu_init(&j, niveau, graine, vitesse);
	terminal_ouvrir();
	t_prec = maintenant();

	for (;;)
	{
		double t = maintenant();
		int touche;

		// Clavier.
		while ((touche = terminal_touche()) != TOUCHE_AUCUNE)
		{
			switch (touche)
			{
			case TOUCHE_HAUT: case 'z': case 'Z': case 'w': case 'W':
				j.pac.voulue = HAUT, demo = false;
				break;
			case TOUCHE_BAS: case 's': case 'S':
				j.pac.voulue = BAS, demo = false;
				break;
			case TOUCHE_GAUCHE: case 'q': case 'Q': case 'a': case 'A':
				j.pac.voulue = GAUCHE, demo = false;
				break;
			case TOUCHE_DROITE: case 'd': case 'D':
				j.pac.voulue = DROITE, demo = false;
				break;
			case 'p': case 'P': case ' ':
				pause = !pause;
				break;
			case 'c': case 'C':
				j.afficher_cibles = !j.afficher_cibles;
				break;
			case 'i': case 'I':
				demo = !demo;
				break;
			case 'n': case 'N':
				jeu_init(&j, niveau, (uint64_t)time(NULL), vitesse);
				pause = false;
				break;
			case 'x': case 'X': case 0x1b:
				terminal_fermer();
				printf("Score : %ld   Record : %ld\n", j.score, j.record);
				return 0;
			default:
				break;
			}
		}

		// Simulation à pas fixe (60 tics par seconde).
		reste += t - t_prec;
		t_prec = t;
		if (reste > 0.25)
			reste = 0.25;
		while (reste >= 1.0 / TICS_PAR_SECONDE)
		{
			reste -= 1.0 / TICS_PAR_SECONDE;
			if (pause)
				continue;
			if (demo)
				pilote_choisir(&j);
			jeu_tic(&j, 1.0 / TICS_PAR_SECONDE);
			if (j.phase == P_GAME_OVER && demo)
				jeu_init(&j, niveau, (uint64_t)time(NULL), vitesse); // la démo recommence seule
		}

		// Affichage (30 images par seconde).
		if (t - t_image >= 1.0 / IMAGES_PAR_SECONDE)
		{
			t_image = t;
			rendu_dessiner(&j, demo, pause);
		}
		dormir(0.004);
	}
}
