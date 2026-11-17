# =============================================================================
#  Makefile - Projet d'Algorithmique 1 : Pacman (ENSSAT)
#
#  Fonctionne sous Linux / WSL et sous MSYS2 MinGW UCRT64 :
#    - Linux / WSL ................ compilation et exécution natives (./pacman) ;
#    - UCRT64 + pacman.o Windows .. compilation native (pacman.exe) ;
#    - UCRT64 + pacman.o Linux .... un objet Linux (ELF) ne peut pas être lié
#      par le gcc de Windows : chaque commande est alors relancée telle quelle
#      dans WSL (« make run LEVEL=2 » exécute « wsl.exe make run LEVEL=2 »).
#
#  Liste des cibles et des options : make help
# =============================================================================

ENGINE  := pacman.o
SRC     := player.c
HEADERS := pacman_dec.h pacman_def.h player.h

# --- Options d'exécution (ex. : make run LEVEL=3 DELAY=30000) ----------------
# Numéro du niveau à jouer (fichier levels/levelN.map)
LEVEL   ?= 1
# Ou directement un fichier de niveau : make run MAP=mon_niveau.map
MAP     ?= levels/level$(LEVEL).map
# Pause entre deux tours, en microsecondes (valeur par défaut du moteur)
DELAY   ?= 100000
# Comportement des fantômes : easy | original
MODE    ?= easy
# Affichage : color | bw  (pas « DISPLAY », variable déjà utilisée par X11)
DISP    ?= color
# Messages de debug de player.c : on | off
DEBUG   ?= off

# --- Options de make test ----------------------------------------------------
# Niveaux joués, nombre de parties par niveau, durée maximale d'une partie (s)
LEVELS  ?= $(sort $(wildcard levels/*.map))
REPEAT  ?= 1
TIMEOUT ?= 60

# --- Fichier à déposer sur Moodle (nom-prenom.c) -----------------------------
RENDU   ?= fridel-escalona-ezequiel.c

ifeq ($(strip $(DELAY)),0)
  $(error DELAY=0 fait planter le moteur (division par zéro) : utilisez DELAY=1 au minimum)
endif

# --- Détection de la plateforme ----------------------------------------------
# uname -s vaut « MINGW64_NT-... » sous UCRT64 (MSYS_NT/CYGWIN_NT ailleurs sous
# Windows) et « Linux » sous Linux et WSL.
# Les 4 premiers octets d'un objet Linux sont 7f 45 4c 46 (« \177ELF »).
UNAME         := $(shell uname -s 2>/dev/null)
ENGINE_IS_ELF := $(findstring 7f 45 4c 46,$(shell od -An -tx1 -N4 $(ENGINE) 2>/dev/null))

ifneq ($(filter MINGW% MSYS% CYGWIN%,$(UNAME)),)
  ifneq ($(ENGINE_IS_ELF),)
    PLATFORM := wsl
  else
    PLATFORM := windows
  endif
else
  PLATFORM := linux
endif


ifeq ($(PLATFORM),wsl)
# =============================================================================
#  Windows + pacman.o Linux : relais vers WSL
#  La première cible demandée relance toute la ligne de commande dans WSL (qui
#  démarre dans le même dossier) ; les éventuelles cibles suivantes ne font
#  rien ici puisqu'elles sont déjà transmises. MSYS2_ARG_CONV_EXCL empêche MSYS2
#  de convertir les chemins passés à wsl.exe (ex. MAP=/home/...).
# =============================================================================
WSL   ?= wsl.exe
GOALS := $(or $(MAKECMDGOALS),all)

.PHONY: $(GOALS)

$(firstword $(GOALS)):
	@echo "[$(ENGINE) est un objet Linux : commande relayée à WSL]"
	@MSYS2_ARG_CONV_EXCL='*' $(WSL) -e make $(GOALS) $(MAKEOVERRIDES)

ifneq ($(words $(GOALS)),1)
$(wordlist 2,$(words $(GOALS)),$(GOALS)):
	@:
endif

else
# =============================================================================
#  Compilation et exécution natives (Linux / WSL, ou MinGW avec pacman.o Windows)
# =============================================================================

# Commande du sujet : gcc -std=c99 -Wall -o pacman pacman.o player.c
# + -lm, car le moteur appelle pow() et sqrt() : sans cette option,
#   l'édition de liens échoue avec le gcc d'Ubuntu 24.04.
CC     := gcc
CFLAGS := -std=c99 -Wall
LDLIBS := -lm

# Avertissements supplémentaires de make check (informatifs, non bloquants).
# -O2 active certaines analyses (variables non initialisées) et -fanalyzer
# cherche des erreurs plus profondes (pointeur NULL, fuite mémoire...).
EXTRA_WARNINGS := -Wextra -pedantic -Wshadow -Wstrict-prototypes -O2 -fanalyzer

# Détection des accès hors du tableau map et des comportements indéfinis
SANITIZE := -g -fno-omit-frame-pointer -fsanitize=address,undefined

ifeq ($(PLATFORM),windows)
  X := .exe
endif
EXE   := pacman$(X)
BUILD := build

# Empreintes SHA-256 des en-têtes fournis, qui ne doivent pas être modifiés
# (calculées sans les \r, pour ignorer un passage en fins de ligne Windows).
HEADERS_SHA256 := \
    pacman_dec.h:ce8a00bf074797f8496faefa0ca1a1947f47df8504b393cd2dbe144f32e0d9d1 \
    pacman_def.h:f9ec37b5cada37beb43e676c9b973d7b50f426a0dd2451d1b473f2f0bc012821 \
    player.h:4b60733b9bac365a354e3d52c04347354ca6833555df219fc06b710df694d02c

# Fonctions qui créent/ouvrent des fichiers ou modifient l'environnement :
# autant de moyens, interdits, de garder de l'information entre deux appels.
FORBIDDEN_FUNCS := fopen freopen fdopen tmpfile tmpnam open openat creat \
                   mkstemp remove rename unlink setenv putenv unsetenv \
                   system popen

# En-têtes de la bibliothèque standard C99
STD_HEADERS := assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h \
               iso646.h limits.h locale.h math.h setjmp.h signal.h stdarg.h \
               stdbool.h stddef.h stdint.h stdio.h stdlib.h string.h \
               tgmath.h time.h wchar.h wctype.h

# Exécutable utilisé par make test (make asan le remplace par la version ASan)
TEST_EXE ?= $(EXE)

.PHONY: all run debug check check-headers check-warnings check-rules lint \
        test asan rendu clean help
.DELETE_ON_ERROR:

# --- Compilation -------------------------------------------------------------
all: $(EXE)

$(EXE): $(ENGINE) $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(ENGINE) $(SRC) $(LDLIBS)

$(BUILD):
	@mkdir -p $@

# --- Exécution ---------------------------------------------------------------
run: $(EXE)
	./$(EXE) -debug $(DEBUG) -display $(DISP) -delay $(DELAY) -mode $(MODE) $(MAP)

debug: DEBUG := on
debug: run

# --- Vérifications -------------------------------------------------------------
check: check-headers check-warnings check-rules lint $(EXE)
	@echo "==> make check réussi."

check-headers:
	@echo "==> En-têtes fournis non modifiés"
	@status=0; \
	for entry in $(HEADERS_SHA256); do \
	    file=$${entry%%:*}; expected=$${entry#*:}; \
	    actual=$$(tr -d '\r' < $$file | sha256sum | cut -d' ' -f1); \
	    if [ "$$actual" = "$$expected" ]; then echo "    OK        $$file"; \
	    else echo "    ERREUR    $$file a été modifié : restaurez la version d'origine"; status=1; fi; \
	done; \
	exit $$status

check-warnings: | $(BUILD)
	@echo "==> Compilation avec les options du sujet ($(CFLAGS)) : aucun warning toléré"
	@$(CC) $(CFLAGS) -Werror -c $(SRC) -o $(BUILD)/player.o
	@echo "    OK        aucun warning"

# Analyse les symboles de player.o : une variable globale ou « static » (même
# déclarée dans une fonction) apparaît comme une donnée, et chaque fonction de
# bibliothèque appelée apparaît comme un symbole non défini (U).
check-rules: check-warnings
	@echo "==> Règles du sujet"
	@status=0; obj=$(BUILD)/player.o; \
	vars=$$(nm $$obj | awk '$$2 ~ /^[bBcCdDgGsSvV]$$/ && $$3 !~ /^\./ && $$3 != "student_name" { print $$3 }'); \
	if [ -n "$$vars" ]; then echo "    ERREUR    variable(s) globale(s) ou static interdite(s) :" $$vars; status=1; \
	else echo "    OK        aucune variable globale ou static (hors student_name)"; fi; \
	consts=$$(nm $$obj | awk '$$2 ~ /^[rR]$$/ && $$3 !~ /^\./ { print $$3 }'); \
	if [ -n "$$consts" ]; then echo "    ATTENTION constante(s) globale(s) ou static :" $$consts "(à faire valider par votre chargé de TP)"; fi; \
	used=$$(nm -u $$obj | awk '{ print $$NF }' | sed -E 's/^(__imp_)?_*//'); \
	bad=""; for f in $(FORBIDDEN_FUNCS); do \
	    if echo "$$used" | grep -qx "$$f"; then bad="$$bad $$f"; fi; \
	done; \
	if [ -n "$$bad" ]; then echo "    ERREUR    fonction(s) de fichiers/environnement interdite(s) :$$bad"; status=1; \
	else echo "    OK        aucun accès fichier ni environnement"; fi; \
	bad=""; for h in $$(sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]\([^>"]*\)[>"].*/\1/p' $(SRC)); do \
	    case " $(STD_HEADERS) $(HEADERS) " in *" $$h "*) ;; *) bad="$$bad $$h";; esac; \
	done; \
	if [ -n "$$bad" ]; then echo "    ATTENTION en-tête(s) hors bibliothèque standard C99 :$$bad"; \
	else echo "    OK        uniquement des en-têtes standard C99 et ceux du sujet"; fi; \
	exit $$status

lint: | $(BUILD)
	@echo "==> Avertissements supplémentaires (informatif, non bloquant)"
	@out=$$($(CC) $(CFLAGS) $(EXTRA_WARNINGS) -c $(SRC) -o $(BUILD)/lint.o 2>&1); \
	if [ -z "$$out" ]; then echo "    OK        rien à signaler"; else printf '%s\n' "$$out"; fi

# --- Tests sur tous les niveaux ----------------------------------------------
# Chaque partie est jouée sans délai ni couleur ; sa sortie complète est gardée
# dans build/logs/ pour voir où et comment Pacman a perdu. Le moteur tire ses
# nombres aléatoires à partir de l'heure (en secondes) : on attend 1 s entre
# deux séries pour que les parties répétées (REPEAT) soient différentes.
test: $(TEST_EXE)
	@mkdir -p $(BUILD)/logs
	@echo "==> $(words $(LEVELS)) niveau(x) x $(REPEAT) partie(s), mode $(MODE), exécutable $(TEST_EXE)"
	@wins=0; games=0; \
	for r in $$(seq 1 $(REPEAT)); do \
	    if [ $$r -gt 1 ]; then sleep 1; fi; \
	    for map in $(LEVELS); do \
	        name=$$(basename $$map .map); log=$(BUILD)/logs/$$name-$$r.log; \
	        timeout $(TIMEOUT) ./$(TEST_EXE) -display bw -delay 1 -mode $(MODE) $$map > $$log 2>&1; rc=$$?; \
	        score=$$(grep -ao 'SCORE: [0-9]*' $$log | tail -n 1 | cut -d' ' -f2); \
	        if grep -aqE 'Sanitizer|runtime error:' $$log; then res="ERREUR MÉMOIRE -> voir $$log"; \
	        elif [ $$rc -eq 124 ]; then res="TIMEOUT (> $(TIMEOUT) s : boucle infinie ?)"; \
	        elif [ $$rc -ge 128 ]; then res="CRASH (signal $$((rc - 128))) -> voir $$log"; \
	        elif grep -aq 'RIP:' $$log; then res="perdu : $$(grep -ao 'RIP: [^!]*' $$log | cut -c6-)"; \
	        elif grep -aqF '|___/' $$log; then res="GAGNÉ"; wins=$$((wins + 1)); \
	        else res="fin inattendue (code $$rc) -> voir $$log"; fi; \
	        games=$$((games + 1)); \
	        printf '    %-12s score %6s   %s\n' "$$name" "$${score:-0}" "$$res"; \
	    done; \
	done; \
	echo "==> Victoires : $$wins / $$games"

asan:
ifeq ($(PLATFORM),windows)
	@echo "AddressSanitizer n'est pas disponible avec MinGW : utilisez Linux / WSL."
	@exit 1
else
	@$(MAKE) --no-print-directory test TEST_EXE=$(BUILD)/pacman-asan
endif

$(BUILD)/pacman-asan: $(ENGINE) $(SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ $(ENGINE) $(SRC) $(LDLIBS)

# --- Rendu -------------------------------------------------------------------
rendu: check
	@cp $(SRC) $(RENDU)
	@$(CC) $(CFLAGS) -Werror -o $(BUILD)/rendu$(X) $(ENGINE) $(RENDU) $(LDLIBS)
	@echo "==> $(RENDU) créé et compilé sans warning : c'est le fichier à déposer sur Moodle."

# --- Nettoyage (ne supprime jamais pacman.o) ---------------------------------
clean:
	rm -rf $(BUILD) pacman pacman.exe $(RENDU)

# --- Aide --------------------------------------------------------------------
help:
	@echo "Pacman - plateforme : $(PLATFORM)"
	@echo ""
	@echo "  make / make all   compile ./$(EXE) (commande du sujet + -lm)"
	@echo "  make run          lance une partie (LEVEL, MAP, DELAY, MODE, DISP, DEBUG)"
	@echo "  make debug        idem avec -debug on (affiche les messages de player.c)"
	@echo "  make check        warnings (-Werror), en-têtes intacts, règles du sujet"
	@echo "  make test         joue tous les niveaux en accéléré : score et résultat"
	@echo "  make asan         comme test, avec AddressSanitizer (Linux / WSL)"
	@echo "  make rendu        check puis copie $(SRC) vers $(RENDU)"
	@echo "  make clean        supprime les fichiers générés (jamais $(ENGINE))"
	@echo ""
	@echo "Options : LEVEL=$(LEVEL)  MAP=$(MAP)  DELAY=$(DELAY) (µs, >= 1)"
	@echo "          MODE=$(MODE) (easy|original)  DISP=$(DISP) (color|bw)  DEBUG=$(DEBUG)"
	@echo "          REPEAT=$(REPEAT)  TIMEOUT=$(TIMEOUT) (s)  RENDU=$(RENDU)"
	@echo ""
	@echo "Exemples : make run LEVEL=3 DELAY=30000"
	@echo "           make test REPEAT=5 MODE=original"

endif
