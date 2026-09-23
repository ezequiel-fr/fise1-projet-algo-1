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
static void directionprinter(direction d);

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
		if (
			(d == NORTH && north) ||
			(d == EAST && east) ||
			(d == SOUTH && south) ||
			(d == WEST && west)
		) ok = true;
	} while (!ok);

	// debug
	if (DEBUG)
	{
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
