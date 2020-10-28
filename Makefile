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
SERVICES_DIR = $(SDIR)/services
PROBA_DIR = $(SERVICES_DIR)/proba
SHARED = $(SRC)/shared
RM = \rm

# Other
FIFO_EXT = fifo

# Cibles
.PHONY: clean cleanb cleano cleanfifo all

all: serveur client

# Serveur
serveur: serveur.o service.o mq.o message.o log.o result.o dictionary.o dict_p.o calcul_simple.o compteur_char.o compteur_mots.o proba_char.o proba_mots.o 
	$(CC) $(CFLAGS) -o $(BIN)/serveur $(OBJ)/$< \
	$(OBJ)/service.o \
	$(OBJ)/mq.o \
	$(OBJ)/message.o \
	$(OBJ)/log.o \
	$(OBJ)/result.o \
	$(OBJ)/dictionary.o \
	$(OBJ)/dict_p.o \
	$(OBJ)/calcul_simple.o \
	$(OBJ)/compteur_char.o \
	$(OBJ)/compteur_mots.o \
	$(OBJ)/proba_char.o \
	$(OBJ)/proba_mots.o  \

# Client
client: client.o mq.o message.o log.o result.o
	$(CC) $(CFLAGS) -o $(BIN)/client $(OBJ)/$< \
	$(OBJ)/mq.o \
	$(OBJ)/message.o \
	$(OBJ)/log.o \
	$(OBJ)/result.o

#génération de ce qui est dans client/ et serveur/
%.o: $(SDIR)/%.c $(SDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@
	
%.o: $(CDIR)/%.c $(CDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@


# dépendances paratagées
%.o: $(SHARED)/%.c $(SHARED)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@

# services
%.o: $(SERVICES_DIR)/%.c $(SERVICES_DIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@
	
# proba dependencies
%.o: $(PROBA_DIR)/%.c $(PROBA_DIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $(OBJ)/$@

# Cleaning
clean: cleanb cleano cleanfifo

cleanb:
	@$(RM) -f $(BIN)/*
	
cleano:
	@find . -type f -name '*.o' -delete
	
cleanfifo:
	-@find /tmp/ -type p -name '*.$(FIFO_EXT)' -delete 2>/dev/null || true

	 
