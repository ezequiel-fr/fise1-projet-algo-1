/*
 * labyrinthe.h - Cartes de jeu (fichiers ASCII de la version initiale ou plan
 * de l'arcade) et labyrinthe d'un niveau.
 *
 * Format ASCII (celui des fichiers levels/levelN.map de la version initiale) :
 *   '*' mur, '.' point, 'O' énergie, ' ' couloir vide, '-' porte de la maison,
 *   '@' Pac-Man, '$' Blinky, '%' Inky, '#' Pinky, '&' Clyde.
 * Sortir par un bord fait réapparaître du côté opposé (tunnels).
 */
#ifndef LABYRINTHE_H
#define LABYRINTHE_H

#include <stdbool.h>

#define LARGEUR_MAX 80
#define HAUTEUR_MAX 48

// Contenu d'une case.
typedef enum
{
	C_VIDE,	   // couloir sans rien à manger
	C_MUR,	   // mur
	C_POINT,   // pac-gomme (10 points)
	C_ENERGIE, // super pac-gomme (50 points, rend les fantômes effrayés)
	C_PORTE,   // porte de la maison des fantômes
	C_MAISON   // intérieur de la maison des fantômes
} case_t;

// Carte telle qu'elle est lue (texte ASCII).
typedef struct
{
	char nom[64];
	int largeur, hauteur;
	char lignes[HAUTEUR_MAX][LARGEUR_MAX + 1];
	bool zones_rouges; // zones où les fantômes ne montent pas (plan de l'arcade uniquement)
} carte_t;

// Labyrinthe d'un niveau, construit à partir d'une carte.
typedef struct
{
	int largeur, hauteur;
	unsigned char c[HAUTEUR_MAX][LARGEUR_MAX];		// case_t
	unsigned char tunnel[HAUTEUR_MAX][LARGEUR_MAX]; // les fantômes y ralentissent
	unsigned char dist_maison[HAUTEUR_MAX][LARGEUR_MAX]; // distance au centre, dans la maison
	unsigned short dist_sortie[HAUTEUR_MAX][LARGEUR_MAX]; // plus court chemin jusqu'à la sortie de la maison
	int points_total, points_restants;

	int pac_x, pac_y;				 // départ de Pac-Man
	int fant_x[4], fant_y[4];		 // départ des fantômes (Blinky, Pinky, Inky, Clyde)
	bool maison;					 // la carte a-t-elle une maison avec une porte ?
	int centre_x, centre_y;			 // case de la maison juste derrière la porte
	int porte_x, porte_y;			 // la porte
	int sortie_x, sortie_y;			 // case juste devant la porte
	int dir_sortie;					 // direction centre -> porte -> sortie (direction_t)
	int fruit_x, fruit_y;			 // apparition des fruits
	bool zones_rouges;
} labyrinthe_t;

bool carte_charger(carte_t *carte, const char *fichier, char *erreur, int taille_erreur);
void carte_arcade(carte_t *carte);

void lab_init(labyrinthe_t *l, const carte_t *carte);
int lab_x(const labyrinthe_t *l, int x);
int lab_y(const labyrinthe_t *l, int y);
case_t lab_case(const labyrinthe_t *l, int x, int y);
bool lab_libre(const labyrinthe_t *l, int x, int y);
bool lab_tunnel(const labyrinthe_t *l, int x, int y);
bool lab_zone_rouge(const labyrinthe_t *l, int x, int y);

#endif
