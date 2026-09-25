/*
 * terminal.c - Terminal en mode brut (POSIX : Linux, WSL, macOS).
 */
#include "terminal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios reglages_origine;
static int ouvert = 0;

/*
 * terminal_fermer : rétablit le terminal (appelée aussi à la sortie du
 * programme et sur Ctrl+C, pour ne jamais laisser le terminal inutilisable).
 */
void terminal_fermer(void)
{
	if (!ouvert)
		return;
	ouvert = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &reglages_origine);
	// Couleurs par défaut, curseur visible, retour à l'écran normal.
	fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
	fflush(stdout);
}

static void sur_signal(int sig)
{
	terminal_fermer();
	signal(sig, SIG_DFL);
	raise(sig);
}

/*
 * terminal_ouvrir : passe en mode brut (touches lues une à une, sans écho ni
 * attente) et bascule sur l'écran alternatif, curseur caché.
 */
void terminal_ouvrir(void)
{
	struct termios brut;

	tcgetattr(STDIN_FILENO, &reglages_origine);
	brut = reglages_origine;
	brut.c_lflag &= ~(ICANON | ECHO);
	brut.c_cc[VMIN] = 0;
	brut.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &brut);
	ouvert = 1;
	atexit(terminal_fermer);
	signal(SIGINT, sur_signal);
	signal(SIGTERM, sur_signal);
	fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
	fflush(stdout);
}

/*
 * terminal_touche : renvoie la prochaine touche disponible, ou TOUCHE_AUCUNE.
 * Les flèches arrivent sous la forme ESC [ A (haut), B, C, D.
 */
int terminal_touche(void)
{
	unsigned char c, seq[2];

	if (read(STDIN_FILENO, &c, 1) != 1)
		return TOUCHE_AUCUNE;
	if (c != 0x1b)
		return c;
	if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1)
		return 0x1b;
	if (seq[0] == '[' || seq[0] == 'O')
	{
		switch (seq[1])
		{
		case 'A': return TOUCHE_HAUT;
		case 'B': return TOUCHE_BAS;
		case 'C': return TOUCHE_DROITE;
		case 'D': return TOUCHE_GAUCHE;
		default: break;
		}
	}
	return TOUCHE_AUCUNE;
}

/*
 * terminal_taille : dimensions du terminal (80 x 24 si inconnues).
 */
void terminal_taille(int *colonnes, int *lignes)
{
	struct winsize w;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
	{
		*colonnes = w.ws_col;
		*lignes = w.ws_row;
	}
	else
	{
		*colonnes = 80;
		*lignes = 24;
	}
}
