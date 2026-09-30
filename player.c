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

// Functions
static void directionprinter(direction d);

// Matrix utils
int **create_matrix(int rows, int cols, int default_value);
void free_matrix(int **matrix, int rows);
void print_matrix(int **matrix, int rows, int cols);

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

		if (
			(d == NORTH && north) ||
			(d == EAST && east) ||
			(d == SOUTH && south) ||
			(d == WEST && west)
		) ok = true;
	} while (!ok);

	// approche par algo glouton

	int possible_d[4] = { north, east, south, west };
	int choice_needed = 0;

	if (north) choice_needed += east + west;
	else if (east) choice_needed += north + south;
	else if (south) choice_needed += east + west;
	else if (west) choice_needed += north + south;

	if (choice_needed)
	{
		// check the adjacent cells at 3 steps in each direction
		const int steps = 3;
		const int VIRGIN_PATH_WEIGHT = 5;

		int **adj = create_matrix(4, steps, 0);

		int direction_weights[4] = { 0, 0, 0, 0 };

		// check the adjacent cells
		for (int i = 1; i <= steps; i++)
		{
			if (north && y - i >= 0 && map[y - i][x] != WALL && map[y - i][x] != DOOR)
			{
				adj[NORTH][i - 1] = map[y - i][x] == VIRGIN_PATH ? VIRGIN_PATH_WEIGHT : 1;

				direction_weights[NORTH] += adj[NORTH][i - 1];
			}

			if (east && x + i < xsize && map[y][x + i] != WALL && map[y][x + i] != DOOR)
			{
				adj[EAST][i - 1] = map[y][x + i] == VIRGIN_PATH ? VIRGIN_PATH_WEIGHT : 1;

				direction_weights[EAST] += adj[EAST][i - 1];
			}

			if (south && y + i < ysize && map[y + i][x] != WALL && map[y + i][x] != DOOR)
			{
				adj[SOUTH][i - 1] = map[y + i][x] == VIRGIN_PATH ? VIRGIN_PATH_WEIGHT : 1;

				direction_weights[SOUTH] += adj[SOUTH][i - 1];
			}

			if (west && x - i >= 0 && map[y][x - i] != WALL && map[y][x - i] != DOOR)
			{
				adj[WEST][i - 1] = map[y][x - i] == VIRGIN_PATH ? VIRGIN_PATH_WEIGHT : 1;

				direction_weights[WEST] += adj[WEST][i - 1];
			}
		}

		if (DEBUG) print_matrix(adj, 4, steps);

		// calculate random path based on the weights of the adjacent cells
		// 
	}

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
		printf("\n");M
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

int random_choice_weighted(int *weights, int size)
{
	int total_weight = 0;

	for (int i = 0; i < size; i += 1)
		total_weight += weights[i];

	// avoid division by zero
	int random_value = rand() % (total_weight || 4);

	for (int i = 0; i < size; i += 1)
	{
		if (random_value < weights[i])
			return i;
		random_value -= weights[i];
	}

	return -1; // should not reach here
}

// Matrix utils
int **create_matrix(int rows, int cols, int default_value)
{
	int **matrix = (int **)malloc(rows * sizeof(int *));

	for (int i = 0; i < rows; i += 1)
	{
		matrix[i] = (int *)malloc(cols * sizeof(int));

		for (int j = 0; j < cols; j += 1)
			matrix[i][j] = default_value;
	}

	return matrix;
}

void free_matrix(int **matrix, int rows)
{
	for (int i = 0; i < rows; i += 1) free(matrix[i]);
	free(matrix);
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
