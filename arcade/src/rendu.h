/*
 * rendu.h - Affichage de la partie dans le terminal (couleurs ANSI).
 */
#ifndef RENDU_H
#define RENDU_H

#include <stdbool.h>

#include "jeu.h"

void rendu_dessiner(const jeu_t *j, bool demo, bool pause);

#endif
