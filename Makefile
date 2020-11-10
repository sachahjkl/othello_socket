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

CLIBS = $(pkg-config --cflags --libs gtk+-3.0)

ifdef $(DEBUG)
	CFLAGS += -g
endif
# GCC warnings that Clang doesn't provide:
ifeq ($(CC),gcc)
    CFLAGS += -Wjump-misses-init -Wlogical-op
endif

# Makefile settings - Can be customized.
SRC = src
BIN = bin
OBJ = obj
CDIR = $(SRC)/client
SDIR = $(SRC)/serveur
UIDIR = $(SRC)/ui
RM = \rm

# Other
FIFO_EXT = fifo

# Cibles
.PHONY: clean cleanb cleano cleanfifo all

all: serveur client

# Serveur
# serveur: serveur.o
# 	$(CC) $(CLIBS) $(CFLAGS) -o $(BIN)/client $(OBJ)/$<

# Client
client:

gui: othello_GUI.o
	$(CC) $(CLIBS) $(CFLAGS) -o $(BIN)/ui $(OBJ)/$<


#génération de ce qui est dans client/ et serveur/
%.o: $(SDIR)/%.c $(SDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@

%.o: $(CDIR)/%.c $(CDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@

%.o: $(UIDIR)/%.c $(UIDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@

# Cleaning
clean: cleanb cleano cleanfifo

cleanb:
	@$(RM) -f $(BIN)/*

cleano:
	@find . -type f -name '*.o' -delete

