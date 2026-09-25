/*
 * jeu.h - État d'une partie de Pac-Man et règles générales (niveaux, score,
 * modes des fantômes, sortie de la maison, collisions).
 */
#ifndef JEU_H
#define JEU_H

#include <stdbool.h>
#include <stdint.h>

#include "labyrinthe.h"

// Directions, dans l'ordre de priorité de l'arcade en cas d'égalité :
// haut, gauche, bas, droite.
typedef enum
{
	HAUT,
	GAUCHE,
	BAS,
	DROITE,
	AUCUNE
} direction_t;

extern const int DX[4], DY[4];
direction_t opposee(direction_t d);

typedef enum
{
	BLINKY, // fantôme rouge : le poursuivant
	PINKY,	// fantôme rose : l'embusqué
	INKY,	// fantôme cyan : l'imprévisible
	CLYDE,	// fantôme orange : le simplet
	NB_FANTOMES
} nom_fantome_t;

// Où en est un fantôme.
typedef enum
{
	F_MAISON,  // attend dans la maison (va et vient de haut en bas)
	F_SORTIE,  // quitte la maison
	F_ACTIF,   // dans le labyrinthe : dispersion, poursuite ou effroi
	F_MANGE,   // mangé : ses yeux retournent à la maison
	F_RENTREE  // ses yeux passent la porte pour se régénérer
} etat_fantome_t;

// Mode global des fantômes, alterné selon un calendrier propre à chaque niveau.
typedef enum
{
	DISPERSION, // « scatter » : chaque fantôme rejoint son coin du labyrinthe
	POURSUITE	// « chase » : chaque fantôme traque Pac-Man à sa manière
} mode_global_t;

typedef struct
{
	nom_fantome_t nom;
	int x, y;			   // case occupée
	direction_t dir;	   // direction du dernier déplacement
	double avance;		   // fraction de case parcourue vers la suivante
	etat_fantome_t etat;
	bool effraye;		   // mode effroi (« frightened ») en cours pour ce fantôme
	bool demi_tour;		   // demi-tour imposé au prochain déplacement
	mode_global_t mode;	   // mode suivi (dispersion ou poursuite) quand il est actif
	int cible_x, cible_y;  // case visée au dernier choix de direction
	int compteur_points;   // compteur personnel de points pour sortir de la maison
} fantome_t;

typedef struct
{
	int x, y;
	direction_t dir;	// direction actuelle
	direction_t voulue; // direction demandée par le joueur (mémorisée jusqu'au virage)
	double avance;
	int pause;			// tics d'arrêt après avoir mangé (comme dans l'arcade)
} pacman_t;

typedef enum
{
	P_PRET,		   // « PRÊT ! » avant le départ
	P_JEU,		   // partie en cours
	P_PAUSE_MANGE, // courte pause après avoir mangé un fantôme
	P_MORT,		   // Pac-Man vient d'être attrapé
	P_NIVEAU_FINI, // tous les points ont été mangés
	P_GAME_OVER
} phase_t;

#define JOURNAL_TAILLE 6
#define JOURNAL_LONGUEUR 112

typedef struct
{
	const carte_t *cartes; // cartes jouées l'une après l'autre (niveau 1 : la première)
	int nb_cartes;
	labyrinthe_t lab;
	pacman_t pac;
	fantome_t f[NB_FANTOMES];

	int niveau;
	long score, record;
	int vies;
	bool vie_bonus; // la vie supplémentaire à 10 000 points a-t-elle été donnée ?

	// Calendrier dispersion / poursuite.
	mode_global_t mode;
	int etape;			// étape du calendrier (0 à 7 : dispersion aux étapes paires)
	double temps_etape; // temps écoulé dans l'étape

	// Mode effroi (déclenché par une énergie).
	double temps_effroi; // temps restant (0 : pas d'effroi)
	int combo;			 // fantômes mangés depuis la dernière énergie

	// Sortie des fantômes de la maison.
	bool compteur_global_actif; // après une vie perdue, un compteur commun remplace les compteurs personnels
	int compteur_global;
	double temps_sans_point; // si Pac-Man ne mange rien pendant un moment, un fantôme sort
	bool elroy_suspendu;	 // après une vie perdue, Blinky n'accélère qu'une fois Clyde sorti

	// Fruit.
	bool fruit_visible;
	double temps_fruit;
	int manges_niveau; // points et énergies mangés dans le niveau

	// Déroulement.
	phase_t phase;
	double chrono;		// temps restant dans la phase en cours (sauf P_JEU)
	double temps_total; // temps de jeu écoulé
	uint64_t alea;		// générateur pseudo-aléatoire (mouvements en effroi)
	double vitesse;		// facteur de vitesse global (1 = arcade)

	// Affichage.
	char journal[JOURNAL_TAILLE][JOURNAL_LONGUEUR];
	int nb_journal;
	bool journal_console; // recopier le journal sur la sortie standard (mode test)
	bool silencieux;	  // pas de journal du tout (simulations de l'IA)
	bool afficher_cibles;
	int popup_x, popup_y, popup_points;
	double popup_temps;

	// Statistiques (tests).
	int stat_fantomes_manges;
	int stat_sorties_apres_effroi;
	int stat_changements_mode;
	int stat_vies_perdues;
	int energies_mangees;
} jeu_t;

// Paramètres d'un niveau (tables de l'arcade).
typedef struct
{
	double pac, pac_effroi;			   // vitesses de Pac-Man (fraction de la vitesse maximale)
	double fant, fant_effroi, tunnel; // vitesses des fantômes
	double elroy1, elroy2;			   // vitesses de Blinky en « Cruise Elroy »
	int elroy1_points, elroy2_points;  // points restants déclenchant Elroy 1 et 2
	double effroi;					   // durée de l'effroi (secondes)
	double calendrier[7];			   // durées des étapes du calendrier (la 8e est infinie)
	int limite_points[NB_FANTOMES];	   // compteurs personnels de sortie
	double limite_temps;			   // délai sans manger avant de libérer un fantôme
	int fruit_points;
	const char *fruit_nom;
} param_niveau_t;

param_niveau_t jeu_param(int niveau);

void jeu_init(jeu_t *j, const carte_t *cartes, int nb_cartes, int niveau, uint64_t graine, double vitesse);
int jeu_echelle(const jeu_t *j, int points_arcade);
void jeu_tic(jeu_t *j, double dt);
void jeu_journal(jeu_t *j, const char *format, ...);
unsigned jeu_alea(jeu_t *j);
void jeu_verifier_collisions(jeu_t *j);
int jeu_elroy(const jeu_t *j);

// Vitesse de référence (100 %) : 75,76 pixels par seconde, soit 9,47 cases par seconde.
#define VITESSE_MAX 9.47

#endif
