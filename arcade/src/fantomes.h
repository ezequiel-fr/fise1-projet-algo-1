/*
 * fantomes.h - Intelligence des quatre fantômes (Blinky, Pinky, Inky, Clyde).
 */
#ifndef FANTOMES_H
#define FANTOMES_H

#include "jeu.h"

void fantomes_placer(jeu_t *j);
void fantomes_avancer(jeu_t *j, double dt);
void fantome_calculer_cible(const jeu_t *j, fantome_t *f);
void fantome_liberer(jeu_t *j, fantome_t *f);
double fantome_vitesse(const jeu_t *j, const fantome_t *f);
const char *fantome_nom(nom_fantome_t n);
const char *fantome_etat_texte(const jeu_t *j, const fantome_t *f);

#endif
