# Pac-Man (arcade) dans le terminal - Linux, WSL ou macOS.
#   make          compile ./pacman
#   make run      joue
#   make ia       regarde l'IA jouer
#   make test     vérifie les règles des fantômes et fait jouer l'IA
#   make clean

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)

.PHONY: all run ia test clean

all: pacman

pacman: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p $@

run: pacman
	./pacman

ia: pacman
	./pacman --ia

test: pacman
	./pacman --test 10 --graine 42

clean:
	rm -rf build pacman
