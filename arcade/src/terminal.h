/*
 * terminal.h - Terminal en mode brut : clavier sans attente, écran alternatif.
 */
#ifndef TERMINAL_H
#define TERMINAL_H

enum
{
	TOUCHE_AUCUNE = -1,
	TOUCHE_HAUT = 1000,
	TOUCHE_BAS,
	TOUCHE_GAUCHE,
	TOUCHE_DROITE
};

void terminal_ouvrir(void);
void terminal_fermer(void);
int terminal_touche(void);
void terminal_taille(int *colonnes, int *lignes);

#endif
