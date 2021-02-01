########################################################################
####################### Makefile ##############################
########################################################################

# Aide mémoire
# $@ 	Le nom de la cible
# $< 	Le nom de la première dépendance
# $^ 	La liste des dépendances
# $? 	La liste des dépendances plus récentes que la cible
# $* 	Le nom du fichier sans suffixe
# lien utile : https://gl.developpez.com/tutoriel/outil/makefile/

# Compiler settings - Can be customized.
DEBUG=1

CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
          -Wformat=2 -Wno-unused-parameter -Wshadow \
          -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
          -Wredundant-decls -Wnested-externs -Wmissing-include-dirs

CLIBS = $(shell pkg-config --cflags --libs gtk+-3.0)

ifdef $(DEBUG)
	CFLAGS += -g
endif
# GCC warnings that Clang doesn't provide:
ifeq ($(CC),gcc)
    CFLAGS += -Wjump-misses-init -Wlogical-op
endif

# Makefile settings - Can be customized.
NAME = othello
SRC = src
BIN = bin
OBJ = obj
CDIR = $(SRC)/client
SDIR = $(SRC)/serveur
UIDIR = $(SRC)/ui
RM = \rm


# Cibles
.PHONY: clean cleanb cleano cleanfifo all

all: $(NAME)

$(NAME): $(NAME).o
	$(CC) $(CLIBS) $(CFLAGS) -o $(BIN)/$@ $(OBJ)/$<

$(NAME).o: $(SRC)/$(NAME).c $(SRC)/$(NAME).h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@ $(CLIBS)

# Cleaning
clean: cleanb cleano cleanfifo

cleanb:
	@$(RM) -f $(BIN)/$(NAME)

cleano:
	@find . -type f -name '*.o' -delete

run: all
	@cd ./bin && ./othello 88888 &

run_adv: all
	@cd ./bin && ./othello 88889 &

run_game: run run_adv
