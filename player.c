// add the needed C libraries below
#include <stdbool.h> // boolean logic
#include <stdlib.h>	 // rand
#include <stdio.h>

// look at the file below for the definition of the direction type
// .h must not be modified!
#include "pacman_dec.h"
#include "pacman_def.h"

// put the student names below (mandatory)
const char *student_name = "Ezequiel FRIDEL ESCALONA";

// put the prototypes of your additional functions/procedures below
// Macros
#define print_boolean(b) printf("%s", b ? "true" : "false")

// Types and structures
typedef struct Position { int x, y; } Position;

// Functions
static void directionprinter(direction d);
static bool is_valid_direction(direction d, bool north, bool east, bool south, bool west);

int random_choice_weighted(int *weights, int size);
int apply_weight(int map_value);

bool backtrack(Position pos, Position obj, char **map, int xsize, int ysize);

// Matrix utils
int **create_matrix(int rows, int cols);
void free_matrix(int **matrix, int rows);

// Print utils
void print_int_arr(int *arr, int size);
void print_matrix(int **matrix, int rows, int cols);

// Backtracking variables
bool **visited = NULL;
Position *path = NULL;

unsigned int path_len = 0;

const int dx[4] = { 0, 0, 1, -1 };
const int dy[4] = { 1, -1, 0, 0 };

// change the pacman function below to build your own player
// your new pacman function can use as many additional functions/procedures as needed; put the code of these functions/procedures *AFTER* the pacman function
direction pacman(
	char **map,					  // the map as a dynamic array of strings, ie of arrays of chars
	int xsize,					  // number of columns of the map
	int ysize,					  // number of lines of the map
	int x,						  // x-position of pacman in the map
	int y,						  // y-position of pacman in the map
	direction lastdirection,	  // last move made by pacman (see pacman.h for the direction type; lastdirection value is -1 at the beginning of the game
	bool energy,				  // is pacman in energy mode?
	int remainingenergymoderounds // number of remaining rounds in energy mode, if energy mode is true
)
{
	direction d;		// the direction to return

	bool north = false; // indicate whether pacman can go north; no by default
	bool east = false;	// indicate whether pacman can go east; no by default
	bool south = false; // indicate whether pacman can go south; no by default
	bool west = false;	// indicate whether pacman can go west; no by default

	bool ok = false;	// turn true when a valid direction is randomly chosen

	// can pacman go north?
	if (y == 0 || (y > 0 && map[y - 1][x] != WALL && map[y - 1][x] != DOOR))
		north = true;
	// can pacman go east?
	if (x == xsize - 1 || (x < xsize - 1 && map[y][x + 1] != WALL && map[y][x + 1] != DOOR))
		east = true;
	// can pacman go south?
	if (y == ysize - 1 || (y < ysize - 1 && map[y + 1][x] != WALL && map[y + 1][x] != DOOR))
		south = true;
	// can pacman go west?
	if (x == 0 || (x > 0 && map[y][x - 1] != WALL && map[y][x - 1] != DOOR))
		west = true;

	// debug
	if (DEBUG)
	{
		printf("Pacman can go: ");

		if (north)
		{
			directionprinter(NORTH);
			printf(" ");
		}
		if (east)
		{
			directionprinter(EAST);
			printf(" ");
		}
		if (south)
		{
			directionprinter(SOUTH);
			printf(" ");
		}
		if (west) directionprinter(WEST);

		printf("\n");
	}

	// guess a direction among the allowed four, until a valid choice is made
	do
	{
		d = rand() % 4; // direction = enum compass that C maps NORTH to 0, EAST to 1,...

		if (north && lastdirection == NORTH && d == SOUTH)
		{
			d = NORTH;
			ok = true;
		}
		else if (east && lastdirection == EAST && d == WEST)
		{
			d = EAST;
			ok = true;
		}
		else if (south && lastdirection == SOUTH && d == NORTH)
		{
			d = SOUTH;
			ok = true;
		}
		else if (west && lastdirection == WEST && d == EAST)
		{
			d = WEST;
			ok = true;
		}

		if (is_valid_direction(d, north, east, south, west)) ok = true;
	} while (!ok);

	// Backtracking method
	visited = (bool **)create_matrix(ysize, xsize);
	path = (Position *)malloc(xsize * ysize * sizeof(Position));

	// Backtrack
	// ...

	// backtrack((Position){ x, y }, obj, map, xsize, ysize);

	// Free backtracking matrix
	free_matrix((int **)(visited), ysize);
	free(path);

	path_len = 0;

	// debug
	if (DEBUG)
	{
		if (x > 1 && x < xsize - 1 && y > 1 && y < ysize - 1)
		{
			printf("Alentours : %c\n", map[x - 1][y]);
			printf("Alentours : %c\n", map[x][y - 1]);
			printf("Alentours : %c\n", map[x + 1][y]);
			printf("Alentours : %c\n", map[x][y + 1]);
		}

		// printf("Last direction: ");
		// directionprinter(lastdirection);
		// printf("\n");
		printf("Next direction: ");
		directionprinter(d);
		printf("\n");
	}

	// answer to the game engine
	return d;
}

// the code of your additional functions/procedures must be put below
static void directionprinter(direction d)
{
	switch (d) {
		case NORTH: printf("NORTH"); break;
		case EAST:  printf("EAST");  break;
		case SOUTH: printf("SOUTH"); break;
		case WEST:  printf("WEST");  break;
	}
}

inline bool is_valid_direction(direction d, bool north, bool east, bool south, bool west)
{
	return false
		|| (north && d == NORTH)
		|| (east && d == EAST)
		|| (south && d == SOUTH)
		|| (west && d == WEST)
	;
}

int random_choice_weighted(int *weights, int size)
{
	// Get the offset (minimum) to use, in order to compute with positive values only
	int offset = weights[0];

	for (int i = 1; i < size; i += 1)
		if (weights[i] < offset) offset = weights[i];

	// Raise every value to be positive
	unsigned int *positive_weights = (unsigned int *)malloc(size * sizeof(int));
	unsigned int total = 0;

	for (int i = 0; i < size; i += 1)
	{
		positive_weights[i] = weights[i] + offset - 1;
		total += positive_weights[i];
	}

	// Get a random value applying weights
	int random_value = rand() % total;
	if (random_value == 0) random_value += 1; // avoid reaching negative/minimum values

	for (int i = 0; i < size; i += 1)
	{
		if (random_value < positive_weights[i]) return i;
		random_value -= positive_weights[i];
	}

	return -1; // should not reach here
}

int apply_weight(int map_value)
{
	if (map_value == VIRGIN_PATH) return 5;
	if (map_value == ENERGY) return 3;
	if (map_value == PATH) return 1;

	if (map_value == GHOST1
		|| map_value == GHOST2
		|| map_value == GHOST3
		|| map_value == GHOST4
	) return -1;

	return 0;
}

bool backtrack(Position pos, Position obj, char **map, int xsize, int ysize)
{
	visited[pos.y][pos.x] = true;
	path[path_len++] = pos;

	if (pos.x == obj.x && pos.y == obj.y)
		return true;

	for (int i = 0; i < 4; i += 1)
	{
		int nx = pos.x + dx[i];
		int ny = pos.y + dy[i];

		bool north = false;
		bool east  = false;
		bool south = false;
		bool west  = false;

		// can pacman go north?
		if (ny == 0 || (ny > 0 && map[ny - 1][nx] != WALL && map[ny - 1][nx] != DOOR))
			north = true;
		// can pacman go east?
		if (nx == xsize - 1 || (nx < xsize - 1 && map[ny][nx + 1] != WALL && map[ny][nx + 1] != DOOR))
			east = true;
		// can pacman go south?
		if (ny == ysize - 1 || (ny < ysize - 1 && map[ny + 1][nx] != WALL && map[ny + 1][nx] != DOOR))
			south = true;
		// can pacman go west?
		if (nx == 0 || (nx > 0 && map[ny][nx - 1] != WALL && map[ny][nx - 1] != DOOR))
			west = true;

		if (is_valid_direction((direction)(i), north, east, south, west))
		{
			Position next = { nx, ny };
			if (backtrack(next, obj, map, xsize, ysize)) return true;
		}
	}

	// Go back in the backtracking
	path_len -= 1;
	visited[pos.y][pos.x] = false;

	return false;
}

// Matrix utils
int **create_matrix(int rows, int cols)
{
	int **matrix = (int **)malloc(rows * sizeof(int *));

	for (int i = 0; i < rows; i += 1)
		matrix[i] = (int *)calloc(cols, sizeof(int));

	return matrix;
}

void free_matrix(int **matrix, int rows)
{
	for (int i = 0; i < rows; i += 1) free(matrix[i]);
	free(matrix);
}

// Print utils
void print_int_arr(int *arr, int size)
{
	printf("[");

	for (int i = 0; i < size;)
	{
		printf("%d", arr[i]);
		if (++i != size) printf(", ");
	}

	printf("]\n");
}

void print_matrix(int **matrix, int rows, int cols)
{
	printf("[\n");

	for (int i = 0; i < rows; i += 1)
	{
		printf("  [");

		for (int j = 0; j < cols; j += 1)
		{
			printf("%d", matrix[i][j]);
			if (j < cols - 1) printf(", ");
		}

		printf("]");

		if (i < rows - 1) printf(",");
		printf("\n");
	}

	printf("]\n");
}
