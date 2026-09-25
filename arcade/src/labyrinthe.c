/*
 * labyrinthe.c - Lecture des cartes ASCII et préparation du labyrinthe
 * (maison des fantômes, tunnels, positions de départ).
 */
#include "labyrinthe.h"

#include <stdio.h>
#include <string.h>

// Directions dans le même ordre que direction_t (jeu.h) : haut, gauche, bas, droite.
static const int LDX[4] = {0, -1, 0, 1};
static const int LDY[4] = {-1, 0, 1, 0};

/*
 * Plan du premier labyrinthe de l'arcade, au format ASCII de la version
 * initiale ('*' mur). Les positions de départ sont ajoutées par carte_arcade.
 */
static const char *PLAN_ARCADE[31] = {
	"****************************",
	"*............**............*",
	"*.****.*****.**.*****.****.*",
	"*O****.*****.**.*****.****O*",
	"*.****.*****.**.*****.****.*",
	"*..........................*",
	"*.****.**.********.**.****.*",
	"*.****.**.********.**.****.*",
	"*......**....**....**......*",
	"******.***** ** *****.******",
	"******.***** ** *****.******",
	"******.**          **.******",
	"******.** ***--*** **.******",
	"******.** *      * **.******",
	"      .   *      *   .      ",
	"******.** *      * **.******",
	"******.** ******** **.******",
	"******.**          **.******",
	"******.** ******** **.******",
	"******.** ******** **.******",
	"*............**............*",
	"*.****.*****.**.*****.****.*",
	"*.****.*****.**.*****.****.*",
	"*O..**.......  .......**..O*",
	"***.**.**.********.**.**.***",
	"***.**.**.********.**.**.***",
	"*......**....**....**......*",
	"*.**********.**.**********.*",
	"*.**********.**.**********.*",
	"*..........................*",
	"****************************",
};

/*
 * carte_arcade : plan de l'arcade (28 x 31), avec Pac-Man et les fantômes à
 * leur place d'origine et les « zones rouges ».
 */
void carte_arcade(carte_t *carte)
{
	snprintf(carte->nom, sizeof carte->nom, "arcade");
	carte->largeur = 28;
	carte->hauteur = 31;
	for (int y = 0; y < carte->hauteur; y++)
		snprintf(carte->lignes[y], sizeof carte->lignes[y], "%s", PLAN_ARCADE[y]);
	carte->lignes[23][13] = '@';
	carte->lignes[11][13] = '$';
	carte->lignes[14][13] = '#';
	carte->lignes[14][11] = '%';
	carte->lignes[14][15] = '&';
	carte->zones_rouges = true;
}

/*
 * carte_charger : lit une carte ASCII (fins de ligne Unix ou Windows). Les
 * lignes plus courtes que la plus longue sont complétées par des murs.
 */
bool carte_charger(carte_t *carte, const char *fichier, char *erreur, int taille_erreur)
{
	FILE *f = fopen(fichier, "r");
	char ligne[512];
	bool pacman = false;
	const char *nom;

	if (f == NULL)
	{
		snprintf(erreur, taille_erreur, "impossible d'ouvrir %s", fichier);
		return false;
	}
	carte->largeur = carte->hauteur = 0;
	carte->zones_rouges = false;
	while (fgets(ligne, sizeof ligne, f) != NULL)
	{
		int n;
		ligne[strcspn(ligne, "\r\n")] = '\0';
		n = (int)strlen(ligne);
		if (carte->hauteur == HAUTEUR_MAX || n > LARGEUR_MAX)
		{
			fclose(f);
			snprintf(erreur, taille_erreur, "%s : carte trop grande (maximum %d x %d)", fichier,
					 LARGEUR_MAX, HAUTEUR_MAX);
			return false;
		}
		snprintf(carte->lignes[carte->hauteur++], LARGEUR_MAX + 1, "%s", ligne);
		if (n > carte->largeur)
			carte->largeur = n;
		if (strchr(ligne, '@') != NULL)
			pacman = true;
	}
	fclose(f);

	// Lignes vides en fin de fichier ignorées, lignes courtes complétées.
	while (carte->hauteur > 0 && carte->lignes[carte->hauteur - 1][0] == '\0')
		carte->hauteur--;
	for (int y = 0; y < carte->hauteur; y++)
	{
		int n = (int)strlen(carte->lignes[y]);
		memset(carte->lignes[y] + n, '*', (size_t)(carte->largeur - n));
		carte->lignes[y][carte->largeur] = '\0';
	}
	if (carte->hauteur < 3 || !pacman)
	{
		snprintf(erreur, taille_erreur, "%s : carte invalide (Pac-Man '@' absent ?)", fichier);
		return false;
	}
	nom = strrchr(fichier, '/');
	snprintf(carte->nom, sizeof carte->nom, "%s", nom ? nom + 1 : fichier);
	return true;
}

int lab_x(const labyrinthe_t *l, int x)
{
	return ((x % l->largeur) + l->largeur) % l->largeur;
}

int lab_y(const labyrinthe_t *l, int y)
{
	return ((y % l->hauteur) + l->hauteur) % l->hauteur;
}

/*
 * lab_case : contenu de la case (x, y), en passant par les bords si besoin.
 */
case_t lab_case(const labyrinthe_t *l, int x, int y)
{
	return (case_t)l->c[lab_y(l, y)][lab_x(l, x)];
}

/*
 * lab_libre : la case est-elle un couloir (praticable par Pac-Man, et par les
 * fantômes hors de la maison) ?
 */
bool lab_libre(const labyrinthe_t *l, int x, int y)
{
	case_t c = lab_case(l, x, y);
	return c == C_VIDE || c == C_POINT || c == C_ENERGIE;
}

bool lab_tunnel(const labyrinthe_t *l, int x, int y)
{
	return l->tunnel[lab_y(l, y)][lab_x(l, x)];
}

/*
 * lab_zone_rouge : cases de l'arcade (au-dessus de la maison et du départ de
 * Pac-Man) d'où les fantômes en dispersion ou en poursuite ne peuvent monter.
 */
bool lab_zone_rouge(const labyrinthe_t *l, int x, int y)
{
	return l->zones_rouges && (x == 12 || x == 15) && (y == 11 || y == 23);
}

/*
 * composante : marque (dans vu) les cases atteignables depuis (x, y) sans
 * traverser de mur ni de porte, et renvoie leur nombre.
 */
static int composante(const labyrinthe_t *l, int x, int y, unsigned char vu[HAUTEUR_MAX][LARGEUR_MAX])
{
	static int file[HAUTEUR_MAX * LARGEUR_MAX];
	int debut = 0, fin = 0;

	memset(vu, 0, sizeof(unsigned char) * HAUTEUR_MAX * LARGEUR_MAX);
	vu[y][x] = 1;
	file[fin++] = y * LARGEUR_MAX + x;
	while (debut < fin)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX;
		debut++;
		for (int d = 0; d < 4; d++)
		{
			int nx = lab_x(l, cx + LDX[d]), ny = lab_y(l, cy + LDY[d]);
			if (!vu[ny][nx] && l->c[ny][nx] != C_MUR && l->c[ny][nx] != C_PORTE)
			{
				vu[ny][nx] = 1;
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}
	return fin;
}

/*
 * trouver_maison : repère la maison des fantômes. Pour la première porte
 * trouvée, la maison est la zone située d'un côté de la porte qui ne contient
 * pas Pac-Man ; la case juste derrière la porte en est le « centre » (les
 * fantômes y passent pour sortir), la case juste devant est la « sortie ».
 */
static void trouver_maison(labyrinthe_t *l)
{
	static unsigned char vu[HAUTEUR_MAX][LARGEUR_MAX];

	l->maison = false;
	for (int y = 0; y < l->hauteur && !l->maison; y++)
		for (int x = 0; x < l->largeur && !l->maison; x++)
		{
			if (l->c[y][x] != C_PORTE)
				continue;
			for (int d = 0; d < 4 && !l->maison; d++)
			{
				int cx = lab_x(l, x + LDX[d]), cy = lab_y(l, y + LDY[d]);
				int sx = lab_x(l, x - LDX[d]), sy = lab_y(l, y - LDY[d]);
				int taille;
				if (l->c[cy][cx] == C_MUR || l->c[cy][cx] == C_PORTE || !lab_libre(l, sx, sy))
					continue;
				taille = composante(l, cx, cy, vu);
				if (vu[l->pac_y][l->pac_x] || taille > l->largeur * l->hauteur / 4)
					continue;
				// Côté maison trouvé : ses cases deviennent l'intérieur de la maison.
				l->maison = true;
				l->centre_x = cx, l->centre_y = cy;
				l->porte_x = x, l->porte_y = y;
				l->sortie_x = sx, l->sortie_y = sy;
				l->dir_sortie = (d + 2) % 4;
				for (int yy = 0; yy < l->hauteur; yy++)
					for (int xx = 0; xx < l->largeur; xx++)
						if (vu[yy][xx])
						{
							if (l->c[yy][xx] == C_POINT || l->c[yy][xx] == C_ENERGIE)
								l->points_total--;
							l->c[yy][xx] = C_MAISON;
						}
			}
		}
}

/*
 * calculer_dist_maison : distance de chaque case de la maison à son centre
 * (BFS), pour guider les fantômes vers la sortie.
 */
static void calculer_dist_maison(labyrinthe_t *l)
{
	static int file[HAUTEUR_MAX * LARGEUR_MAX];
	int debut = 0, fin = 0;

	memset(l->dist_maison, 255, sizeof l->dist_maison);
	if (!l->maison)
		return;
	l->dist_maison[l->centre_y][l->centre_x] = 0;
	file[fin++] = l->centre_y * LARGEUR_MAX + l->centre_x;
	while (debut < fin)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX;
		debut++;
		for (int d = 0; d < 4; d++)
		{
			int nx = lab_x(l, cx + LDX[d]), ny = lab_y(l, cy + LDY[d]);
			if (l->c[ny][nx] == C_MAISON && l->dist_maison[ny][nx] == 255)
			{
				l->dist_maison[ny][nx] = (unsigned char)(l->dist_maison[cy][cx] + 1);
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}
}

/*
 * calculer_dist_sortie : longueur du plus court chemin de chaque couloir
 * jusqu'à la case de sortie de la maison (BFS), utilisée par les yeux des
 * fantômes mangés pour rentrer.
 */
static void calculer_dist_sortie(labyrinthe_t *l)
{
	static int file[HAUTEUR_MAX * LARGEUR_MAX];
	int debut = 0, fin = 0;

	memset(l->dist_sortie, 0xff, sizeof l->dist_sortie);
	if (!l->maison)
		return;
	l->dist_sortie[l->sortie_y][l->sortie_x] = 0;
	file[fin++] = l->sortie_y * LARGEUR_MAX + l->sortie_x;
	while (debut < fin)
	{
		int cx = file[debut] % LARGEUR_MAX, cy = file[debut] / LARGEUR_MAX;
		debut++;
		for (int d = 0; d < 4; d++)
		{
			int nx = lab_x(l, cx + LDX[d]), ny = lab_y(l, cy + LDY[d]);
			if (lab_libre(l, nx, ny) && l->dist_sortie[ny][nx] == 0xffff)
			{
				l->dist_sortie[ny][nx] = (unsigned short)(l->dist_sortie[cy][cx] + 1);
				file[fin++] = ny * LARGEUR_MAX + nx;
			}
		}
	}
}

/*
 * lab_init : construit le labyrinthe d'un niveau à partir d'une carte.
 */
void lab_init(labyrinthe_t *l, const carte_t *carte)
{
	const char FANTOMES[4] = {'$', '#', '%', '&'}; // Blinky, Pinky, Inky, Clyde

	memset(l, 0, sizeof *l);
	l->largeur = carte->largeur;
	l->hauteur = carte->hauteur;
	l->zones_rouges = carte->zones_rouges;
	for (int i = 0; i < 4; i++)
		l->fant_x[i] = l->fant_y[i] = -1;

	// Contenu des cases et positions de départ.
	for (int y = 0; y < l->hauteur; y++)
		for (int x = 0; x < l->largeur; x++)
		{
			char ch = carte->lignes[y][x];
			l->c[y][x] = C_VIDE;
			switch (ch)
			{
			case '*': l->c[y][x] = C_MUR; break;
			case '.': l->c[y][x] = C_POINT; l->points_total++; break;
			case 'O': l->c[y][x] = C_ENERGIE; l->points_total++; break;
			case '-': l->c[y][x] = C_PORTE; break;
			case '@': l->pac_x = x, l->pac_y = y; break;
			default:
				for (int i = 0; i < 4; i++)
					if (ch == FANTOMES[i])
						l->fant_x[i] = x, l->fant_y[i] = y;
				break;
			}
		}

	trouver_maison(l);
	calculer_dist_maison(l);
	calculer_dist_sortie(l);

	// Fantôme absent de la carte : il part du centre de la maison, ou, sans
	// maison, du côté opposé du labyrinthe.
	for (int i = 0; i < 4; i++)
		if (l->fant_x[i] < 0)
		{
			l->fant_x[i] = l->maison ? l->centre_x : lab_x(l, l->pac_x + l->largeur / 2);
			l->fant_y[i] = l->maison ? l->centre_y : l->pac_y;
		}

	// Tunnels : couloirs qui débouchent sur les deux bords opposés, près des bords.
	for (int y = 0; y < l->hauteur; y++)
		for (int x = 0; x < l->largeur; x++)
		{
			bool ligne = l->c[y][0] != C_MUR && l->c[y][l->largeur - 1] != C_MUR;
			bool colonne = l->c[0][x] != C_MUR && l->c[l->hauteur - 1][x] != C_MUR;
			l->tunnel[y][x] = l->c[y][x] != C_MUR &&
							  ((ligne && (x <= 5 || x >= l->largeur - 6)) ||
							   (colonne && (y <= 3 || y >= l->hauteur - 4)));
		}

	// Fruit : première case de couloir sous la maison, dans l'axe de son centre
	// (sinon au départ de Pac-Man).
	l->fruit_x = l->pac_x;
	l->fruit_y = l->pac_y;
	if (l->maison)
		for (int y = l->centre_y + 1; y < l->hauteur; y++)
			if (lab_libre(l, l->centre_x, y))
			{
				l->fruit_x = l->centre_x;
				l->fruit_y = y;
				break;
			}

	l->points_restants = l->points_total;
}
