/*
 * rendu.h - Affichage de la partie dans le terminal (couleurs ANSI).
 */
#ifndef RENDU_H
#define RENDU_H

#include <stdbool.h>

#include "jeu.h"

typedef enum
{
	STYLE_ASCII, // caractères de la version initiale, une colonne par case
	STYLE_BLOCS	 // murs pleins, deux colonnes par case
} style_t;

void rendu_dessiner(const jeu_t *j, style_t style, bool ia, bool pause);

#endif
