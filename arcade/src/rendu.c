/*
 * rendu.c - Affichage de la partie dans le terminal.
 *
 * Chaque case occupe deux colonnes, pour que le labyrinthe ne paraisse pas
 * écrasé. L'image entière est construite dans un tampon puis écrite d'un coup
 * (curseur replacé en haut à gauche) pour éviter le scintillement. Le panneau
 * d'informations (modes, cibles, journal) s'affiche à droite du labyrinthe si
 * le terminal est assez large, sinon en dessous.
 */
#include "rendu.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fantomes.h"
#include "terminal.h"

// Couleurs (palette 256 couleurs).
static const int COULEUR_FANTOME[NB_FANTOMES] = {196, 213, 51, 214};
enum
{
	COULEUR_MUR = 19,
	COULEUR_MUR_FLASH = 255,
	COULEUR_PORTE = 218,
	COULEUR_POINT = 223,
	COULEUR_PACMAN = 226,
	COULEUR_EFFROI = 27,
	COULEUR_FLASH = 255,
	COULEUR_FRUIT = 196,
	COULEUR_TEXTE = 250,
	COULEUR_TITRE = 229
};

enum { TAMPON = 1 << 16, PANNEAU_LIGNES = 40, PANNEAU_LONGUEUR = 256 };

static char tampon[TAMPON];
static size_t lg;
static char panneau[PANNEAU_LIGNES][PANNEAU_LONGUEUR];
static int nb_panneau;

static void ecrire(const char *format, ...)
{
	va_list args;
	int n;

	if (lg >= TAMPON)
		return;
	va_start(args, format);
	n = vsnprintf(tampon + lg, TAMPON - lg, format, args);
	va_end(args);
	if (n > 0)
		lg += (size_t)n < TAMPON - lg ? (size_t)n : TAMPON - lg - 1;
}

static void ligne_panneau(const char *format, ...)
{
	va_list args;

	if (nb_panneau >= PANNEAU_LIGNES)
		return;
	va_start(args, format);
	vsnprintf(panneau[nb_panneau++], PANNEAU_LONGUEUR, format, args);
	va_end(args);
}

/*
 * couleur_fantome : couleur d'un fantôme selon son état (bleu en effroi,
 * clignotant blanc pendant les 2 dernières secondes).
 */
static int couleur_fantome(const jeu_t *j, const fantome_t *f)
{
	if (f->effraye)
	{
		if (j->temps_effroi < 2.0 && (int)(j->temps_effroi * 5) % 2 == 0)
			return COULEUR_FLASH;
		return COULEUR_EFFROI;
	}
	return COULEUR_FANTOME[f->nom];
}

/*
 * dessiner_case : dessine la case (x, y), personnages compris.
 */
static void dessiner_case(const jeu_t *j, int x, int y)
{
	case_t c = j->lab.c[y][x];
	bool clignote = (int)(j->temps_total * 4) % 2 == 0;

	// Points gagnés (sur deux cases).
	if (j->popup_temps > 0 && y == j->popup_y && (x == j->popup_x || x == j->popup_x + 1))
	{
		char texte[8];
		snprintf(texte, sizeof texte, "%-4d", j->popup_points);
		ecrire("\x1b[1;38;5;51m%.2s\x1b[0m", texte + (x == j->popup_x ? 0 : 2));
		return;
	}

	// Pac-Man.
	if (x == j->pac.x && y == j->pac.y && j->phase != P_GAME_OVER)
	{
		if (j->phase == P_MORT && (int)(j->chrono * 8) % 2 == 0)
			ecrire("\x1b[38;5;%dm* \x1b[0m", COULEUR_PACMAN);
		else
			ecrire("\x1b[1;38;5;%dm@ \x1b[0m", COULEUR_PACMAN);
		return;
	}

	// Fantômes (le dernier de la liste est dessiné par-dessus).
	for (int i = NB_FANTOMES - 1; i >= 0; i--)
	{
		const fantome_t *f = &j->f[i];
		if (f->x != x || f->y != y || j->phase == P_NIVEAU_FINI)
			continue;
		if (f->etat == F_MANGE || f->etat == F_RENTREE)
			ecrire("\x1b[1;97moo\x1b[0m");
		else
			ecrire("\x1b[1;38;5;%dmM \x1b[0m", couleur_fantome(j, f));
		return;
	}

	// Fruit.
	if (j->fruit_visible && x == FRUIT_X && y == FRUIT_Y)
	{
		ecrire("\x1b[1;38;5;%dm%% \x1b[0m", COULEUR_FRUIT);
		return;
	}

	// Cibles des fantômes (touche C).
	if (j->afficher_cibles && c != C_MUR)
		for (int i = 0; i < NB_FANTOMES; i++)
		{
			const fantome_t *f = &j->f[i];
			if (f->etat == F_ACTIF && !f->effraye && f->cible_x == x && f->cible_y == y)
			{
				ecrire("\x1b[1;38;5;%dmx \x1b[0m", COULEUR_FANTOME[i]);
				return;
			}
		}

	switch (c)
	{
	case C_MUR:
		ecrire("\x1b[48;5;%dm  \x1b[0m",
			   j->phase == P_NIVEAU_FINI && (int)(j->chrono * 4) % 2 ? COULEUR_MUR_FLASH : COULEUR_MUR);
		break;
	case C_POINT:
		ecrire("\x1b[38;5;%dm· \x1b[0m", COULEUR_POINT);
		break;
	case C_ENERGIE:
		if (clignote || j->phase != P_JEU)
			ecrire("\x1b[1;38;5;%dm● \x1b[0m", COULEUR_POINT);
		else
			ecrire("  ");
		break;
	case C_PORTE:
		ecrire("\x1b[38;5;%dm──\x1b[0m", COULEUR_PORTE);
		break;
	default:
		ecrire("  ");
		break;
	}
}

/*
 * remplir_panneau : informations de jeu (modes, cibles, journal, touches).
 */
static void remplir_panneau(const jeu_t *j, bool demo)
{
	param_niveau_t p = jeu_param(j->niveau);
	const char *nom_mode = j->mode == DISPERSION ? "DISPERSION" : "POURSUITE";

	nb_panneau = 0;
	ligne_panneau("\x1b[1;38;5;%dmVies\x1b[0m  %d      \x1b[1;38;5;%dmFruit\x1b[0m  %s (%d)",
				  COULEUR_TITRE, j->vies, COULEUR_TITRE, p.fruit_nom, p.fruit_points);
	ligne_panneau("Points restants : %d / %d", j->lab.points_restants, j->lab.points_total);
	ligne_panneau("");
	ligne_panneau("\x1b[1;38;5;%dmMode global\x1b[0m", COULEUR_TITRE);
	if (j->temps_effroi > 0)
		ligne_panneau("  \x1b[1;38;5;%dmEFFROI\x1b[0m encore %.1f s (calendrier suspendu en %s)",
					  COULEUR_EFFROI, j->temps_effroi, nom_mode);
	else if (j->etape < 7)
		ligne_panneau("  %s, étape %d/8, encore %.1f s", nom_mode, j->etape + 1,
					  p.calendrier[j->etape] - j->temps_etape);
	else
		ligne_panneau("  %s, étape 8/8 (définitive)", nom_mode);
	ligne_panneau("");
	ligne_panneau("\x1b[1;38;5;%dmFantômes\x1b[0m", COULEUR_TITRE);
	for (int i = 0; i < NB_FANTOMES; i++)
	{
		const fantome_t *f = &j->f[i];
		char cible[32] = "";
		if (f->etat == F_ACTIF && !f->effraye)
			snprintf(cible, sizeof cible, "cible (%d,%d)", f->cible_x, f->cible_y);
		else if (f->etat == F_MANGE)
			snprintf(cible, sizeof cible, "cible la porte");
		ligne_panneau("  \x1b[1;38;5;%dm%-7s\x1b[0m \x1b[38;5;%dm%-20s\x1b[0m %s", COULEUR_FANTOME[i],
					  fantome_nom(f->nom), couleur_fantome(j, f), fantome_etat_texte(j, f), cible);
	}
	ligne_panneau("");
	ligne_panneau("\x1b[1;38;5;%dmJournal\x1b[0m", COULEUR_TITRE);
	for (int i = 0; i < JOURNAL_TAILLE; i++)
		ligne_panneau("  \x1b[38;5;%dm%s\x1b[0m", COULEUR_TEXTE, i < j->nb_journal ? j->journal[i] : "");
	ligne_panneau("");
	ligne_panneau("\x1b[38;5;244mFlèches/ZQSD/WASD : diriger   P : pause   C : cibles\x1b[0m");
	ligne_panneau("\x1b[38;5;244mI : pilote auto%s   N : nouvelle partie   X : quitter\x1b[0m",
				  demo ? " (actif)" : "");
}

/*
 * message_central : texte affiché sous la maison (ligne 17), centré sur 12
 * cases. largeur = nombre de colonnes affichées par le texte.
 */
static bool message_central(const jeu_t *j, bool pause, const char **texte, int *largeur, int *couleur)
{
	*couleur = COULEUR_PACMAN;
	if (pause)
		*texte = "PAUSE", *largeur = 5;
	else if (j->phase == P_PRET)
		*texte = "PRÊT !", *largeur = 6;
	else if (j->phase == P_GAME_OVER)
		*texte = "GAME  OVER", *largeur = 10, *couleur = 196;
	else
		return false;
	return true;
}

void rendu_dessiner(const jeu_t *j, bool demo, bool pause)
{
	int colonnes, lignes, marge;
	bool a_droite;
	const char *msg;
	int msg_largeur, msg_couleur;
	bool avec_msg = message_central(j, pause, &msg, &msg_largeur, &msg_couleur);

	terminal_taille(&colonnes, &lignes);
	a_droite = colonnes >= 2 * LARGEUR + 4 + 60;
	remplir_panneau(j, demo);

	lg = 0;
	ecrire("\x1b[H");
	ecrire(" \x1b[1;38;5;%dmSCORE\x1b[0m %-8ld  \x1b[1;38;5;%dmRECORD\x1b[0m %-8ld  "
		   "\x1b[1;38;5;%dmNIVEAU\x1b[0m %d%s\x1b[K\r\n",
		   COULEUR_TITRE, j->score, COULEUR_TITRE, j->record, COULEUR_TITRE, j->niveau,
		   demo ? "   \x1b[38;5;244m(démo)\x1b[0m" : "");

	for (int y = 0; y < HAUTEUR; y++)
	{
		ecrire(" ");
		for (int x = 0; x < LARGEUR; x++)
		{
			// Message central : remplace les cases 8 à 19 de la ligne 17.
			if (avec_msg && y == FRUIT_Y && x >= 8 && x < 20)
			{
				if (x == 8)
				{
					int avant = (24 - msg_largeur) / 2, apres = 24 - msg_largeur - avant;
					ecrire("%*s\x1b[1;38;5;%dm%s\x1b[0m%*s", avant, "", msg_couleur, msg, apres, "");
				}
				continue;
			}
			dessiner_case(j, x, y);
		}
		if (a_droite && y < nb_panneau)
			ecrire("   %s", panneau[y]);
		ecrire("\x1b[K\r\n");
	}
	if (!a_droite)
		for (int i = 0; i < nb_panneau; i++)
			ecrire(" %s\x1b[K\r\n", panneau[i]);
	ecrire("\x1b[J");
	marge = write(STDOUT_FILENO, tampon, lg) < 0;
	(void)marge;
	(void)lignes;
}
