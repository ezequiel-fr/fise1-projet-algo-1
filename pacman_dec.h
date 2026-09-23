#include <stdbool.h> // for the bool type

#ifndef PACMAN_DEC_H
#define PACMAN_DEC_H
// ascii characters used for drawing levels
extern const char PACMAN;      // ascii used for pacman
extern const char WALL;        // ascii used for the walls
extern const char PATH;        // ascii used for the explored paths
extern const char DOOR;        // ascii used for the ghosts' door
extern const char VIRGIN_PATH; // ascii used for the unexplored paths
extern const char ENERGY;      // ascii used for the energizers
extern const char GHOST1;      // ascii used for the ghost 1
extern const char GHOST2;      // ascii used for the ghost 2
extern const char GHOST3;      // ascii used for the ghost 3
extern const char GHOST4;      // ascii used for the ghost 4

// reward (in points) when eating dots/energizers
extern const int VIRGIN_PATH_SCORE; // reward for eating a dot
extern const int ENERGY_SCORE;      // reward for eating an energizer

// debug
extern bool DEBUG; // debug mode
#endif
