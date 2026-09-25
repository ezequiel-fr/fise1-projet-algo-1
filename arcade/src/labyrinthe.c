/*
 * labyrinthe.c - Plan du labyrinthe et règles de déplacement associées.
 */
#include "labyrinthe.h"

/*
 * Plan du premier labyrinthe de l'arcade :
 *   '#' mur, '.' point, 'o' énergie, ' ' couloir vide, '-' porte,
 *   'H' intérieur de la maison, 'V' néant (hors labyrinthe).
 * La ligne 14 est le tunnel : ses deux extrémités communiquent.
 */
static const char *PLAN[HAUTEUR] = {
	"############################",
	"#............##............#",
	"#.####.#####.##.#####.####.#",
	"#o####.#####.##.#####.####o#",
	"#.####.#####.##.#####.####.#",
	"#..........................#",
	"#.####.##.########.##.####.#",
	"#.####.##.########.##.####.#",
	"#......##....##....##......#",
	"######.##### ## #####.######",
	"VVVVV#.##### ## #####.#VVVVV",
	"VVVVV#.##          ##.#VVVVV",
	"VVVVV#.## ###--### ##.#VVVVV",
	"######.## #HHHHHH# ##.######",
	"      .   #HHHHHH#   .      ",
	"######.## #HHHHHH# ##.######",
	"VVVVV#.## ######## ##.#VVVVV",
	"VVVVV#.##          ##.#VVVVV",
	"VVVVV#.## ######## ##.#VVVVV",
	"######.## ######## ##.######",
	"#............##............#",
	"#.####.#####.##.#####.####.#",
	"#.####.#####.##.#####.####.#",
	"#o..##.......  .......##..o#",
	"###.##.##.########.##.##.###",
	"###.##.##.########.##.##.###",
	"#......##....##....##......#",
	"#.##########.##.##########.#",
	"#.##########.##.##########.#",
	"#..........................#",
	"############################",
};

/*
 * lab_init : remplit le labyrinthe à partir du plan et compte les points.
 */
void lab_init(labyrinthe_t *l)
{
	l->points_total = 0;
	for (int y = 0; y < HAUTEUR; y++)
		for (int x = 0; x < LARGEUR; x++)
		{
			switch (PLAN[y][x])
			{
			case '#': l->c[y][x] = C_MUR; break;
			case '.': l->c[y][x] = C_POINT; l->points_total++; break;
			case 'o': l->c[y][x] = C_ENERGIE; l->points_total++; break;
			case '-': l->c[y][x] = C_PORTE; break;
			case 'H': l->c[y][x] = C_MAISON; break;
			case 'V': l->c[y][x] = C_NEANT; break;
			default:  l->c[y][x] = C_VIDE; break;
			}
		}
	l->points_restants = l->points_total;
}

/*
 * lab_x : ramène une colonne dans le labyrinthe (passage par le tunnel).
 */
int lab_x(int x)
{
	return ((x % LARGEUR) + LARGEUR) % LARGEUR;
}

/*
 * lab_case : contenu de la case (x, y) ; hors des lignes du plan : néant.
 */
case_t lab_case(const labyrinthe_t *l, int x, int y)
{
	if (y < 0 || y >= HAUTEUR)
		return C_NEANT;
	return l->c[y][lab_x(x)];
}

/*
 * lab_libre : la case (x, y) est-elle un couloir du labyrinthe ? C'est le cas
 * pour Pac-Man et pour les fantômes hors de leur maison (la porte et
 * l'intérieur de la maison sont gérés à part).
 */
bool lab_libre(const labyrinthe_t *l, int x, int y)
{
	case_t c = lab_case(l, x, y);
	return c == C_VIDE || c == C_POINT || c == C_ENERGIE;
}

/*
 * lab_tunnel : la case fait-elle partie du tunnel, où les fantômes ralentissent ?
 */
bool lab_tunnel(int x, int y)
{
	x = lab_x(x);
	return y == TUNNEL_Y && (x <= 5 || x >= 22);
}

/*
 * lab_zone_rouge : cases (au-dessus de la maison et du départ de Pac-Man) où,
 * dans l'arcade, les fantômes en dispersion ou en poursuite ne peuvent pas
 * choisir de monter.
 */
bool lab_zone_rouge(int x, int y)
{
	return (x == 12 || x == 15) && (y == 11 || y == 23);
}
