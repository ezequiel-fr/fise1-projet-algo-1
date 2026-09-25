/*
 * labyrinthe.h - Le labyrinthe de Pac-Man (plan de l'arcade, 28 x 31 cases).
 */
#ifndef LABYRINTHE_H
#define LABYRINTHE_H

#include <stdbool.h>

#define LARGEUR 28
#define HAUTEUR 31

// Contenu d'une case.
typedef enum
{
	C_VIDE,	   // couloir sans rien à manger
	C_MUR,	   // mur
	C_POINT,   // pac-gomme (10 points)
	C_ENERGIE, // super pac-gomme (50 points, rend les fantômes effrayés)
	C_PORTE,   // porte de la maison des fantômes
	C_MAISON,  // intérieur de la maison des fantômes
	C_NEANT	   // hors du labyrinthe (autour des tunnels)
} case_t;

typedef struct
{
	case_t c[HAUTEUR][LARGEUR];
	int points_total;	  // points + énergies au début du niveau
	int points_restants;  // points + énergies restant à manger
} labyrinthe_t;

// Cases remarquables (colonne, ligne).
enum
{
	SORTIE_X = 13, SORTIE_Y = 11,	// case devant la porte : sortie / retour des fantômes
	MAISON_X = 13, MAISON_Y = 14,	// centre de la maison
	MAISON_HAUT = 13, MAISON_BAS = 15,
	PACMAN_X = 13, PACMAN_Y = 23,	// départ de Pac-Man
	FRUIT_X = 13, FRUIT_Y = 17,		// apparition des fruits
	TUNNEL_Y = 14					// ligne du tunnel
};

void lab_init(labyrinthe_t *l);
int lab_x(int x);
case_t lab_case(const labyrinthe_t *l, int x, int y);
bool lab_libre(const labyrinthe_t *l, int x, int y);
bool lab_tunnel(int x, int y);
bool lab_zone_rouge(int x, int y);

#endif
