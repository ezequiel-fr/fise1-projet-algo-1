/*
 * rendu.c - Affichage de la partie dans le terminal.
 *
 * Deux styles :
 *   - ASCII (par défaut) : les caractères de la version initiale, une colonne
 *     par case ('*' mur, '.' point, 'O' énergie, '-' porte, '@' Pac-Man,
 *     '$' Blinky, '#' Pinky, '%' Inky, '&' Clyde) ;
 *   - blocs : murs pleins et deux colonnes par case.
 * L'image entière est construite dans un tampon puis écrite d'un coup
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
static const char CAR_FANTOME[NB_FANTOMES] = {'$', '#', '%', '&'};
enum
{
	COULEUR_MUR = 19,
	COULEUR_MUR_ASCII = 33,
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

enum { TAMPON = 1 << 17, PANNEAU_LIGNES = 40, PANNEAU_LONGUEUR = 256 };

static char tampon[TAMPON];
static size_t lg;
static char panneau[PANNEAU_LIGNES][PANNEAU_LONGUEUR];
static int nb_panneau;

static void ecrire(const char *format, ...)
{
	va_list args;
	int n;

	if (lg >= TAMPON - 1)
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
 * symbole : écrit une case de largeur 1 (ASCII) ou 2 (blocs) : le caractère
 * c en couleur, suivi d'une espace en style blocs.
 */
static void symbole(style_t style, int couleur, bool gras, const char *c)
{
	ecrire("\x1b[%s38;5;%dm%s%s\x1b[0m", gras ? "1;" : "", couleur, c, style == STYLE_BLOCS ? " " : "");
}

/*
 * dessiner_case : dessine la case (x, y), personnages compris.
 */
static void dessiner_case(const jeu_t *j, style_t style, int x, int y)
{
	case_t c = (case_t)j->lab.c[y][x];
	bool clignote = (int)(j->temps_total * 4) % 2 == 0;
	int largeur = style == STYLE_BLOCS ? 2 : 1;

	// Points gagnés, écrits sur les cases suivantes.
	if (j->popup_temps > 0 && y == j->popup_y && x >= j->popup_x && (x - j->popup_x) * largeur < 4)
	{
		char texte[8];
		snprintf(texte, sizeof texte, "%-4d", j->popup_points);
		ecrire("\x1b[1;38;5;51m%.*s\x1b[0m", largeur, texte + (x - j->popup_x) * largeur);
		return;
	}

	// Pac-Man ('_' quand il vient d'être mangé, comme dans la version initiale).
	if (x == j->pac.x && y == j->pac.y && j->phase != P_GAME_OVER)
	{
		symbole(style, COULEUR_PACMAN, true, j->phase == P_MORT && (int)(j->chrono * 8) % 2 == 0 ? "_" : "@");
		return;
	}

	// Fantômes (Blinky dessiné par-dessus les autres).
	for (int i = NB_FANTOMES - 1; i >= 0; i--)
	{
		const fantome_t *f = &j->f[i];
		char car[2] = {CAR_FANTOME[i], '\0'};
		if (f->x != x || f->y != y || j->phase == P_NIVEAU_FINI)
			continue;
		if (f->etat == F_MANGE || f->etat == F_RENTREE)
			symbole(style, 255, true, "\""); // les yeux
		else
			symbole(style, couleur_fantome(j, f), true, style == STYLE_BLOCS ? "M" : car);
		return;
	}

	// Fruit.
	if (j->fruit_visible && x == j->lab.fruit_x && y == j->lab.fruit_y)
	{
		symbole(style, COULEUR_FRUIT, true, "+");
		return;
	}

	// Cibles des fantômes (touche C).
	if (j->afficher_cibles && c != C_MUR)
		for (int i = 0; i < NB_FANTOMES; i++)
		{
			const fantome_t *f = &j->f[i];
			if (f->etat == F_ACTIF && !f->effraye && f->cible_x == x && f->cible_y == y)
			{
				symbole(style, COULEUR_FANTOME[i], true, "x");
				return;
			}
		}

	switch (c)
	{
	case C_MUR:
	{
		bool flash = j->phase == P_NIVEAU_FINI && (int)(j->chrono * 4) % 2;
		if (style == STYLE_BLOCS)
			ecrire("\x1b[48;5;%dm  \x1b[0m", flash ? COULEUR_MUR_FLASH : COULEUR_MUR);
		else
			symbole(style, flash ? COULEUR_MUR_FLASH : COULEUR_MUR_ASCII, false, "*");
		break;
	}
	case C_POINT:
		symbole(style, COULEUR_POINT, false, style == STYLE_BLOCS ? "·" : ".");
		break;
	case C_ENERGIE:
		if (clignote || j->phase != P_JEU)
			symbole(style, COULEUR_POINT, true, style == STYLE_BLOCS ? "●" : "O");
		else
			ecrire("%*s", largeur, "");
		break;
	case C_PORTE:
		if (style == STYLE_BLOCS) // la porte occupe toute la largeur de la case
			ecrire("\x1b[38;5;%dm──\x1b[0m", COULEUR_PORTE);
		else
			symbole(style, COULEUR_PORTE, false, "-");
		break;
	default:
		ecrire("%*s", largeur, "");
		break;
	}
}

/*
 * remplir_panneau : informations de jeu (modes, cibles, journal, touches).
 */
static void remplir_panneau(const jeu_t *j, bool ia)
{
	param_niveau_t p = jeu_param(j->niveau);
	const char *nom_mode = j->mode == DISPERSION ? "DISPERSION" : "POURSUITE";

	nb_panneau = 0;
	ligne_panneau("\x1b[1;38;5;%dmCarte\x1b[0m %s   \x1b[1;38;5;%dmVies\x1b[0m %d   \x1b[1;38;5;%dmFruit\x1b[0m %s (%d)",
				  COULEUR_TITRE, j->cartes[(j->niveau - 1) % j->nb_cartes].nom, COULEUR_TITRE, j->vies,
				  COULEUR_TITRE, p.fruit_nom, p.fruit_points);
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
		ligne_panneau("  \x1b[1;38;5;%dm%c %-7s\x1b[0m \x1b[38;5;%dm%-20s\x1b[0m %s", COULEUR_FANTOME[i],
					  CAR_FANTOME[i], fantome_nom(f->nom), couleur_fantome(j, f), fantome_etat_texte(j, f), cible);
	}
	ligne_panneau("");
	ligne_panneau("\x1b[1;38;5;%dmJournal\x1b[0m", COULEUR_TITRE);
	for (int i = 0; i < JOURNAL_TAILLE; i++)
		ligne_panneau("  \x1b[38;5;%dm%s\x1b[0m", COULEUR_TEXTE, i < j->nb_journal ? j->journal[i] : "");
	ligne_panneau("");
	ligne_panneau("\x1b[38;5;244mFlèches/ZQSD/WASD : diriger   P : pause   C : cibles\x1b[0m");
	ligne_panneau("\x1b[38;5;244mI : IA %s   N : nouvelle partie   X : quitter\x1b[0m",
				  ia ? "(active)" : "(inactive)");
}

void rendu_dessiner(const jeu_t *j, style_t style, bool ia, bool pause)
{
	int colonnes, lignes, largeur = style == STYLE_BLOCS ? 2 : 1;
	bool a_droite;
	const char *message = "";

	terminal_taille(&colonnes, &lignes);
	(void)lignes;
	a_droite = colonnes >= largeur * j->lab.largeur + 4 + 60;
	remplir_panneau(j, ia);

	if (pause)
		message = "\x1b[1;38;5;226mPAUSE\x1b[0m";
	else if (j->phase == P_PRET)
		message = "\x1b[1;38;5;226mPRÊT !\x1b[0m";
	else if (j->phase == P_GAME_OVER)
		message = "\x1b[1;38;5;196mGAME OVER\x1b[0m   (N : nouvelle partie)";
	else if (j->phase == P_NIVEAU_FINI)
		message = "\x1b[1;38;5;226mNIVEAU TERMINÉ !\x1b[0m";

	lg = 0;
	ecrire("\x1b[H");
	ecrire(" \x1b[1;38;5;%dmSCORE\x1b[0m %-8ld \x1b[1;38;5;%dmRECORD\x1b[0m %-8ld \x1b[1;38;5;%dmNIVEAU\x1b[0m %-3d %s%s\x1b[K\r\n",
		   COULEUR_TITRE, j->score, COULEUR_TITRE, j->record, COULEUR_TITRE, j->niveau,
		   ia ? "\x1b[38;5;244m(IA) \x1b[0m" : "", message);

	for (int y = 0; y < j->lab.hauteur; y++)
	{
		ecrire(" ");
		for (int x = 0; x < j->lab.largeur; x++)
			dessiner_case(j, style, x, y);
		if (a_droite && y < nb_panneau)
			ecrire("   %s", panneau[y]);
		ecrire("\x1b[K\r\n");
	}
	for (int i = a_droite ? j->lab.hauteur : 0; i < nb_panneau; i++)
		ecrire("%*s %s\x1b[K\r\n", a_droite ? largeur * j->lab.largeur + 3 : 0, "", panneau[i]);
	ecrire("\x1b[J");
	if (write(STDOUT_FILENO, tampon, lg) < 0)
		return;
}
