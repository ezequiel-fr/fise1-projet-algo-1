# Pac-Man (arcade) dans le terminal - Linux, WSL ou macOS.
#   make          compile ./pacman
#   make run      joue
#   make demo     regarde le pilote automatique jouer
#   make test     vérifie les cibles des fantômes et simule 20 parties
#   make clean

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)

.PHONY: all run demo test clean

all: pacman

pacman: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p $@

run: pacman
	./pacman

demo: pacman
	./pacman --demo

test: pacman
	./pacman --test 20 --graine 42

clean:
	rm -rf build pacman
