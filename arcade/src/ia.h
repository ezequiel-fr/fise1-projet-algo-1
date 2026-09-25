/*
 * ia.h - Intelligence artificielle de Pac-Man.
 */
#ifndef IA_H
#define IA_H

#include "jeu.h"

// Mémoire de l'IA entre deux tics : elle ne recalcule son choix qu'en
// arrivant sur une nouvelle case.
typedef struct
{
	int x, y;		  // case où la dernière décision a été prise
	phase_t phase;	  // phase de jeu à ce moment-là
	int niveau;
	direction_t choix;
	bool valide;
	long score_precedent; // score à la décision précédente
	int sans_gain;		  // décisions successives sans rien manger
	long decisions;	  // statistiques
	double duree_totale;
} ia_t;

void ia_init(ia_t *ia);
void ia_piloter(ia_t *ia, jeu_t *j);
direction_t ia_choisir(const jeu_t *j, int urgence);

#endif
