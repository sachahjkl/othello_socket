
#define _GNU_SOURCE
#include "othello.h"

#include <gtk/gtk.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_DATA_SIZE 256
#define BOARD_SIZE 8
#define MAX_TITLE_LENGTH 45

#define LABEL_YOU "Vous"
#define LABEL_ADVERSARY "Adversaire"
#define MESSAGE_YOU_WON "Fin de la partie.\n\n Vous avez gagné!!!"
#define MESSAGE_YOU_LOST "Fin de la partie.\n\nVous avez perdu!"
#define MESSAGE_STALEMATE "Impossible de jouer.\n\n :)"

typedef enum color { BLACK, WHITE, EMPTY, OUT } color_e;
typedef enum type { COORD, COLOR, END, NO_MOVE_POSSIBLE } type_e;

typedef struct request_data {
  unsigned short type;  // 0 pour la couleur, 1 pour les coordonnées x, y
  unsigned short x;
  unsigned short y;
  unsigned short color;
} request_s;

/* Variables globales */
color_e board[BOARD_SIZE][BOARD_SIZE];  // tableau associe au damier
color_e my_color;

int port;  // numero port passe a l'appel

char *adv_addr, *adv_port;  // Info sur adversaire

pthread_t thread_id;  // Id du thread fils gerant connexion socket

int sockfd, newsockfd = -1;     // descripteurs de socket
int addr_size;                  // taille adresse
struct sockaddr *adv_sockaddr;  // structure pour stocker adresse adversaire

fd_set master, read_fds, write_fds;  // ensemble de socket pour toutes les
                                     // sockets actives avec select
int fdmax;                           // utilise pour select

int status;
struct addrinfo hints, *servinfo, *p;

/* Variables globales associées à l'interface graphique */
GtkBuilder *p_builder = NULL;
GError *p_err = NULL;
GtkWidget *p_win = NULL;

/** DEBUT DES ENTÊTES DES FONCTIONS **/

/**
 * Fonction permettant de changer l'image d'une case du damier
 * (indiqué par sa colonne et sa row)
 * @param col la colonne de la case du damier
 * @param lig la row de la case du damier
 * @param couleur_j la couleur du joueur (0 noir, 1 blanc)
 */
void update_cell_image(int col, int row, color_e p_color);

/**
 * Fonction permettant changer nom joueur blanc dans cadre Score
 * @param texte nom du J1 (blanc)
 * @usefulfor interface graphique
 */
void set_label_black(const char *texte);

/**
 * Fonction permettant changer nom joueur noir dans cadre Score
 * @param texte nom du J2 (noir)
 * @usefulfor interface graphique
 */
void set_label_white(const char *texte);

/**
 * Fonction permettant de changer score joueur blanc dans cadre Score
 * @param score le score du J1 (blanc)
 * @see get_score_J1(void)
 */
void set_score_black(int score);

/**
 * Fonction permettant de récupérer score joueur blanc dans cadre Score
 * @return le score du J1 (blanc)
 * @see set_score_J1(int)
 */
int get_score_black(void);

/**
 * Fonction permettant de changer score joueur noir dans cadre Score
 * @param score le score du J2 (noir)
 * @see #get_score_J2(void)
 */
void set_score_white(int score);

/**
 * Fonction permettant de récupérer score joueur noir dans cadre Score
 * @return le score du J2 (noir)
 * @see #set_score_J2(int)
 */
int get_score_white(void);

/**
 * Fonction transformant les coordonnees du damier graphique
 * en indexes pour matrice du damier
 */
void coord_to_indexes(const gchar *coord, int *col, int *lig);

/**
 * Fonction transformant les indexes pour matrice
 * du damier en coordonnees du damier graphique
 */
void indexes_to_coord(int col, int lig, char *coord);

/**
 * Fonction appelee lors du clic
 * sur une case du damier
 */
static void on_click_cell(GtkWidget *p_case);

/**
 * Fonction retournant texte du champs adresse
 * du serveur de l'interface graphique
 * @return l'adresse du serveur
 */
char *read_adv_server(void);

/**
 * Fonction retournant le texte du champs port
 * du serveur de l'interface graphique
 * @return le port du serveur
 */
char *read_server_port(void);

/**
 * Fonction retournant le texte du champs login
 * de l'interface graphique
 * @return le login
 */
char *read_login(void);

/**
 * Fonction retournant le texte du champs adresse
 * du cadre Joueurs de l'interface graphique
 * @return l'adresse de l'adversaire
 */
char *read_adv_addr(void);

/**
 * Fonction retournant le texte du champs port
 * du cadre Joueurs de l'interface graphique
 * @return le port de l'adversaire
 */
char *read_adv_port(void);

/**
 * Fonction permettant d'afficher
 * soit #affiche_fenetre_gagne(void)
 * soit #affiche_fenetre_perdu(void)
 * en fonction des cas et en fonction
 * de la couleur qui jouer
 */
void show_lose_or_win(void);

/**
 * Fonction affichant une boite de dialogue
 * si la partie est gagnée
 * @usefulfor interface graphique
 */
void show_win(void);

/**
 * Fonction affichant une boite de dialogue
 * si la partie est perdue
 * @usefulfor interface graphique
 */
void show_lost(void);

/**
 * Fonction affichant une boite de dialogue
 * si le joueur ne peut pas jouer
 * @usefulfor interface graphique
 */
void show_stalemate(void);

/**
 * Fonction appelée lors du clic du bouton Se connecter
 * @usefulfor interface graphique
 */
static void on_click_connect(GtkWidget *b);

/**
 * Fonction desactivant bouton demarrer partie
 * @usefulfor interface graphique
 */
void disable_button_start(void);

/**
 * Fonction appelee lors du clic du bouton Demarrer partie
 * @usefulfor interface graphique
 */
static void on_click_start(GtkWidget *b);

/**
 * Fonction desactivant les cases du damier
 * pour attendre que l'utilisateur adverse joue (ignore les clics du joueurs
 * courant)
 * @usefulfor interface graphique
 * @see #degele_damier(void)
 */
void freeze_board(void);

/**
 * Fonction activant les cases du damier
 * pour que l'utilisateur puisse jouer
 * @usefulfor interface graphique
 * @see #gele_damier(void)
 */
void unfreeze_board(void);

/**
 * Fonction permettant d'initialiser le plateau de jeu
 * @usefulfor interface graphique
 */
void init_interface(void);

/**
 * Fonction reinitialisant la liste des joueurs sur l'interface graphique
 * @usefulfor interface graphique
 */
void reset_liste_joueurs(void);

/**
 * Fonction permettant d'ajouter un joueur
 * dans la liste des joueurs sur l'interface graphique
 * @param login le login du joueur
 * @param adresse l'adresse ip du joueur
 * @param port le port du joueur
 * @usefulfor interface graphique
 */
void add_player_to_list(char *login, char *adress, char *port_s);

/**
 * Fonction exécutée par le thread gérant
 * les communications à travers la socket
 */
static void *f_com_socket(void *p_arg);

// ajout des entetes qui ne sont pas du prof

/**
 * Fonction vérifiant si une case est jouable
 * 1/ en vérifiant qu'on ne depasse pas la taille du damier
 * 2/ que l'on joue en se basant sur son voisin
 */
bool playable(int col, int lig, color_e p_color);

/**
 * Fonction permettant de changer la couleur de la case
 */
void set_case(int col, int row, color_e p_color);

/**
 * Fonction qui retourne si on doit changer la row
 */
bool change_ligne(int col, int row, int dx, int dy, color_e p_color);

/**
 * Fonction changeant la couleur des cases
 * @see #changement_case(int, int, int, int, int)
 */
void update_cells(int col, int row, color_e p_color);

/**
 * Fonction changeant la couleur des cases dans la direction choisie
 */
void update_cell_directed(int col, int row, int dx, int dy, color_e p_color);

/**
 * Fonction permettant de savoir
 * si on peut encore placer des pièces
 */
bool game_is_over(void);

/**
 * Fonction permettant de savoir
 * si les deux joueurs ne peuvent poser
 * plus de pièces
 * @see http://www.ffothello.org/images/jeu/regles/figure-10.jpg
 */
bool early_game_over(void);

/**
 * Fonction permettant de savoir
 * si le joueur ne peut pas jouer
 */
bool stalemate(void);

/**
 * Fonction permettant de retourner la couleur de la case
 */
color_e get_board_color(int col, int row);

/**
 * Fonction retournant la couleur du joueur courant
 */
color_e get_player_color(void);

/**
 * Fonction initialisant la couleur du joueur courant
 */
void set_player_couleur(color_e p_color);

/**
 * Fonction retournant la couleur de l'adversaire
 */
color_e get_adv_color(void);

/**
 * Fonction retournant la couleur adverse
 * @param la couleur
 * @return la couleur adverse à @param
 */
color_e get_opposite_color(color_e p_color);

/** FIN DES ENTÊTES DES FONCTIONS **/

/**
 * Fonction transformant les coordonnees du damier graphique
 * en indexes pour matrice du damier
 */
void coord_to_indexes(const gchar *coord, int *col, int *lig) {
  char *c;
  c = malloc(2 * sizeof(char));

  c = strncpy(c, coord, 1);
  c[1] = '\0';

  if (strcmp(c, "A") == 0)
    *col = 0;
  else if (strcmp(c, "B") == 0)
    *col = 1;
  else if (strcmp(c, "C") == 0)
    *col = 2;
  else if (strcmp(c, "D") == 0)
    *col = 3;
  else if (strcmp(c, "E") == 0)
    *col = 4;
  else if (strcmp(c, "F") == 0)
    *col = 5;
  else if (strcmp(c, "G") == 0)
    *col = 6;
  else if (strcmp(c, "H") == 0)
    *col = 7;

  *lig = atoi(coord + 1) - 1;
}

/**
 * Fonction transformant les indexes pour matrice
 * du damier en coordonnees du damier graphique
 */
void indexes_to_coord(int col, int lig, char *coord) {
  char c;

  if (col == 0)
    c = 'A';
  else if (col == 1)
    c = 'B';
  else if (col == 2)
    c = 'C';
  else if (col == 3)
    c = 'D';
  else if (col == 4)
    c = 'E';
  else if (col == 5)
    c = 'F';
  else if (col == 6)
    c = 'G';
  else if (col == 7)
    c = 'H';

  sprintf(coord, "%c%d", c, lig + 1);
}

// Fin ajout

/**
 * Fonction permettant de changer l'image d'une case du damier
 * (indiqué par sa colonne et sa row)
 * @param col la colonne de la case du damier
 * @param lig la row de la case du damier
 * @param couleur_j la couleur du joueur (0 noir, 1 blanc)
 */
void update_cell_image(int col, int row, color_e p_color) {
  char *coord;

  coord = malloc(3 * sizeof(char));

  indexes_to_coord(col, row, coord);

  if (p_color == WHITE) {
    // image pion blanc
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)),
                            IMG_WHITE_TOKEN);
  } else {
    // image pion noir
    gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)),
                            IMG_BLACK_TOKEN);
  }
}

/**
 * Fonction permettant changer nom joueur blanc dans cadre Score
 * @param texte nom du J1 (blanc)
 * @usefulfor interface graphique
 */
void set_label_black(const char *texte) {
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J1")),
                     texte);
}

/**
 * Fonction permettant changer nom joueur noir dans cadre Score
 * @param texte nom du J2 (noir)
 * @usefulfor interface graphique
 */
void set_label_white(const char *texte) {
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J2")),
                     texte);
}

/**
 * Fonction permettant de changer score joueur blanc dans cadre Score
 * @param score le score du J1 (blanc)
 * @see #get_score_J1(void)
 */
void set_score_black(int score) {
  char *s;

  s = malloc(5 * sizeof(char));
  sprintf(s, "%d", score);

  gtk_label_set_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ1")), s);
}

/**
 * Fonction permettant de récupérer score joueur blanc dans cadre Score
 * @return le score du J1 (blanc)
 * @see #set_score_J1(int)
 */
int get_score_black(void) {
  const gchar *c;

  c = gtk_label_get_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ1")));

  return atoi(c);
}

/**
 * Fonction permettant de changer score joueur noir dans cadre Score
 * @param score le score du J2 (noir)
 * @see #get_score_J2(void)
 */
void set_score_white(int score) {
  char *s;

  s = malloc(5 * sizeof(char));
  sprintf(s, "%d", score);

  gtk_label_set_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ2")), s);
}

/**
 * Fonction permettant de récupérer score joueur noir dans cadre Score
 * @return le score du J2 (noir)
 * @see #set_score_J2(int)
 */
int get_score_white(void) {
  const gchar *c;

  c = gtk_label_get_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ2")));

  return atoi(c);
}

/**
 * Fonction vérifiant si une case est jouable
 * 1/ en vérifiant qu'on ne depasse pas la taille du damier
 * 2/ que l'on joue en se basant sur son voisin
 */
bool playable(int col, int row, color_e p_color) {
  return (get_board_color(col, row) == EMPTY &&
          (change_ligne(col, row, -1, -1, p_color) ||
           change_ligne(col, row, 0, -1, p_color) ||
           change_ligne(col, row, 1, -1, p_color) ||
           change_ligne(col, row, -1, 0, p_color) ||
           change_ligne(col, row, 1, 0, p_color) ||
           change_ligne(col, row, -1, 1, p_color) ||
           change_ligne(col, row, 0, 1, p_color) ||
           change_ligne(col, row, 1, 1, p_color)));
}

/**
 * Fonction permettant de changer la couleur de la case
 */
void set_case(int col, int row, color_e p_color) {
  // on test la couleur de la case
  if (get_opposite_color(p_color) == get_board_color(col, row)) {
    if (p_color == WHITE) {
      set_score_black(get_score_black() + 1);
      set_score_white(get_score_white() - 1);
    } else if (p_color == BLACK) {
      set_score_black(get_score_black() - 1);
      set_score_white(get_score_white() + 1);
    }

  } else {
    if (p_color == WHITE) {
      set_score_black(get_score_black() + 1);
    } else if (p_color == BLACK) {
      set_score_white(get_score_white() + 1);
    }
  }
  // On insere le pion de la couleur du joueur courant
  board[col][row] = p_color;
  printf("changement couleur de %d;%d en %d\n", col, row, p_color);
  // On change l'image de la case en transformant son numéro en img
  update_cell_image(col, row, p_color);
}

bool change_ligne(int col, int row, int dx, int dy, color_e p_color) {
  int cell_count = 0;
  // on fait un parcours tant que la couleur de la selection de direction est
  // différente
  col += dx;
  row += dy;
  while (get_board_color(col, row) == get_opposite_color(p_color)) {
    // on increment le compteur
    cell_count++;
    // on increment le parcours vers la direction choisie
    col += dx;
    row += dy;
  }
  // si la couleur qu'on vient de parcourir est la même que la couleur du joueur
  // qui vient de jouer ET que le compteur est supéreur a 1(pour dire que c'est
  // pas un voisin de la meme couleur)
  return ((get_board_color(col, row) == p_color) && (cell_count > 0));
}

/**
 * Fonction changeant la couleur des cases
 * @see #changement_case(int, int, int, int, int)
 */
void update_cells(int col, int row, color_e p_color) {
  if (change_ligne(col, row, 1, 1, p_color)) {
    update_cell_directed(col, row, 1, 1, p_color);
  }
  if (change_ligne(col, row, -1, -1, p_color)) {
    update_cell_directed(col, row, -1, -1, p_color);
  }
  if (change_ligne(col, row, -1, 1, p_color)) {
    update_cell_directed(col, row, -1, 1, p_color);
  }
  if (change_ligne(col, row, 0, -1, p_color)) {
    update_cell_directed(col, row, 0, -1, p_color);
  }
  if (change_ligne(col, row, 1, 0, p_color)) {
    update_cell_directed(col, row, 1, 0, p_color);
  }
  if (change_ligne(col, row, 1, -1, p_color)) {
    update_cell_directed(col, row, 1, -1, p_color);
  }
  if (change_ligne(col, row, 0, 1, p_color)) {
    update_cell_directed(col, row, 0, 1, p_color);
  }
  if (change_ligne(col, row, -1, 0, p_color)) {
    update_cell_directed(col, row, -1, 0, p_color);
  }
}

/**
 * Fonction changeant la couleur des cases dans la direction choisie
 */
void update_cell_directed(int col, int row, int dx, int dy, color_e p_color) {
  col += dx;
  row += dy;
  while (get_board_color(col, row) == get_opposite_color(p_color)) {
    // on change la couleur de la case et on met à jour le score
    set_case(col, row, p_color);
    // on increment le parcours vers la direction choisie
    col += dx;
    row += dy;
  }
}

/**
 * Fonction permettant de savoir
 * si on peut encore placer des pièces
 */
bool game_is_over(void) {
  int black_score, white_score, max_score;
  black_score = get_score_black();
  white_score = get_score_white();
  max_score = BOARD_SIZE * BOARD_SIZE;
  return ((black_score + white_score) == max_score);
}

/**
 * Fonction permettant de savoir
 * si les deux joueurs ne peuvent poser
 * plus de pièces
 * @see http://www.ffothello.org/images/jeu/regles/figure-10.jpg
 */
bool early_game_over(void) {
  int i, j;
  for (i = 0; i < BOARD_SIZE; ++i) {
    for (j = 0; j < BOARD_SIZE; ++j) {
      if ((get_board_color(i, j) == EMPTY) &&
          (playable(i, j, WHITE) || playable(i, j, BLACK)))
        return false;
    }
  }
  return true;
}

/**
 * Fonction permettant de savoir
 * si le joueur ne peut pas jouer
 */
bool stalemate(void) {
  int i, j;
  for (i = 0; i < BOARD_SIZE; ++i) {
    for (j = 0; j < BOARD_SIZE; ++j) {
      if ((get_board_color(i, j) == EMPTY) &&
          (playable(i, j, get_player_color())))
        return false;
    }
  }
  return true;
}

/**
 * Fonction permettant de retourner la couleur de la case
 */
color_e get_board_color(int col, int row) {
  if ((col < 0) || (row < 0) || (col >= BOARD_SIZE) || (row >= BOARD_SIZE)) {
    return OUT;
  } else {
    return board[col][row];
  }
}

/**
 * Fonction retournant la couleur du joueur courant
 */
color_e get_player_color(void) { return my_color; }

/**
 * Fonction initialisant la couleur du joueur courant
 */
void set_player_couleur(color_e p_color) { my_color = p_color; }

/**
 * Fonction retournant la couleur de l'adversaire
 */
color_e get_adv_color(void) {
  if (get_player_color() == WHITE) {
    return BLACK;
  }
  if (get_player_color() == BLACK) {
    return WHITE;
  }
  return EMPTY;
}

/**
 * Fonction retournant la couleur adverse
 * @param la couleur
 * @return la couleur adverse à @param
 */
color_e get_opposite_color(color_e p_color) {
  if (p_color == WHITE) {
    return BLACK;
  }
  if (p_color == BLACK) {
    return WHITE;
  }
  return EMPTY;
}

/**
 * Fonction appelee lors du clic
 * sur une case du damier
 */
static void on_click_cell(GtkWidget *p_case) {
  int col, row;
  char buf[MAX_DATA_SIZE];

  // ajout initiasation requete
  request_s req;

  // Traduction coordonnees damier en indexes matrice damier
  coord_to_indexes(
      gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))),
      &col, &row);

  if (game_is_over()) {
    // les joueurs ne peuvent pas jouer car le jeu est rempli
    // donc partie finie
    freeze_board();

    // préparation message et envoie sur socket à adversaire
    req.type = htons((unsigned short)END);

    printf("Requête à envoyer %u\n", req.type);
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%u", req.type);

    printf("Envoyer requête %s\n", buf);
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_lose_or_win();
  } else if (early_game_over()) {
    // les deux joueurs ne peuvent pas placer le pion
    // donc partie finie
    // les places vident vont au gagnant
    int place_vide =
        (BOARD_SIZE * BOARD_SIZE) - (get_score_black() + get_score_white());
    if (get_score_black() > get_score_white()) {
      set_score_black(get_score_black() + place_vide);
    } else {
      set_score_white(get_score_white() + place_vide);
    }

    // préparation message et envoie sur socket à adversaire
    req.type = htons((unsigned short)END);

    printf("Requête à envoyer %u\n", req.type);
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%u", req.type);

    printf("Envoyer requête %s\n", buf);
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_lose_or_win();
  } else if (stalemate()) {
    // le joueur ne peut pas placer le pion mais l'autre si

    freeze_board();

    // préparation message et envoie sur socket à adversaire
    req.type = htons((unsigned short)NO_MOVE_POSSIBLE);

    printf("Requête à envoyer %u\n", req.type);
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%u", req.type);

    printf("Envoyer requête %s\n", buf);
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_stalemate();
  } else {
    if (playable(col, row, get_player_color())) {
      // change la case
      set_case(col, row, get_player_color());
      freeze_board();
      // déterminer pions dont la couleur modifie + score
      update_cells(col, row, get_player_color());

      printf("Info à envoyer : %u %u\n", col, row);

      // préparation message et envoie sur socket à adversaire
      req.type = htons((unsigned short)COORD);
      req.x = htons((unsigned short)col);
      req.y = htons((unsigned short)row);
      printf("Requête à envoyer %u : %u %u\n", req.type, req.x, req.y);
      bzero(buf, MAX_DATA_SIZE);
      snprintf(buf, MAX_DATA_SIZE, "%u, %u, %u", req.type, req.x, req.y);

      printf("Envoyer requête %s\n", buf);
      if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
        perror("send");
        close(newsockfd);
      }
    }
  }
}

/**
 * Fonction retournant texte du champs adresse
 * du serveur de l'interface graphique
 * @return l'adresse du serveur
 */
char *read_adv_server(void) {
  GtkWidget *entry_addr_srv;

  entry_addr_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_adr");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

/**
 * Fonction retournant le texte du champs port
 * du serveur de l'interface graphique
 * @return le port du serveur
 */
char *read_server_port(void) {
  GtkWidget *entry_port_srv;

  entry_port_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

/**
 * Fonction retournant le texte du champs login
 * de l'interface graphique
 * @return le login
 */
char *read_login(void) {
  GtkWidget *entry_login;

  entry_login = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_login");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_login));
}

/**
 * Fonction retournant le texte du champs adresse
 * du cadre Joueurs de l'interface graphique
 * @return l'adresse de l'adversaire
 */
char *read_adv_addr(void) {
  GtkWidget *entry_addr_j2;

  entry_addr_j2 =
      (GtkWidget *)gtk_builder_get_object(p_builder, "entry_addr_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_j2));
}

/**
 * Fonction retournant le texte du champs port
 * du cadre Joueurs de l'interface graphique
 * @return le port de l'adversaire
 */
char *read_adv_port(void) {
  GtkWidget *entry_port_j2;

  entry_port_j2 =
      (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_j2));
}

/**
 * Fonction permettant d'afficher
 * soit #affiche_fenetre_gagne(void)
 * soit #affiche_fenetre_perdu(void)
 * en fonction des cas et en fonction
 * de la couleur qui jouer
 */
void show_lose_or_win(void) {
  if (get_player_color() == WHITE) {
    if (get_score_black() > get_score_white()) {
      show_win();
    } else {
      show_lost();
    }
  } else if (get_player_color() == BLACK) {
    if (get_score_white() > get_score_black()) {
      show_win();
    } else {
      show_lost();
    }
  }
}

/**
 * Fonction affichant une boite de dialogue
 * si la partie est gagnée
 * @usefulfor interface graphique
 */
void show_win(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_YOU_WON);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

/**
 * Fonction affichant une boite de dialogue
 * si la partie est perdue
 * @usefulfor interface graphique
 */
void show_lost(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_YOU_LOST);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

/**
 * Fonction affichant une boite de dialogue
 * si le joueur ne peut pas jouer
 * @usefulfor interface graphique
 */
void show_stalemate(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_STALEMATE);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

/**
 * Fonction appelée lors du clic du bouton Se connecter
 * @usefulfor interface graphique
 */
static void on_click_connect(GtkWidget *b) {
  /*
   * TO DO
   */
}

/**
 * Fonction desactivant bouton demarrer partie
 * @usefulfor interface graphique
 */
void disable_button_start(void) {
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);
}

// Ajout

/**
 * Fonction appelee lors du clic du bouton Demarrer partie
 * @usefulfor interface graphique
 */
static void on_click_start(GtkWidget *b) {
  if (newsockfd == -1) {
    // Desactivation bouton demarrer partie
    gtk_widget_set_sensitive(
        (GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);

    // Recuperation  adresse et port adversaire au format chaines caracteres
    adv_addr = read_adv_addr();
    adv_port = read_adv_port();

    printf("[%d] addr adv : %s\n", port, adv_addr);
    printf("[%d] port adv : %s\n", port, adv_port);

    // utilise le signal SIGUSR1 pour lancer la socket
    pthread_kill(thread_id, SIGUSR1);
  }
}

// Fin ajout

/**
 * Fonction desactivant les cases du damier
 * pour attendre que l'utilisateur adverse joue (ignore les clics du joueurs
 * courant)
 * @usefulfor interface graphique
 * @see #degele_damier(void)
 */
void freeze_board(void) {
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH1"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH2"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH3"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH4"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH5"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH6"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH7"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG8"), FALSE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH8"), FALSE);
}

/**
 * Fonction activant les cases du damier
 * pour que l'utilisateur puisse jouer
 * @usefulfor interface graphique
 * @see #gele_damier(void)
 */
void unfreeze_board(void) {
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH1"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH2"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH3"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH4"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH5"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH6"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH7"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG8"), TRUE);
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH8"), TRUE);
}

/**
 * Fonction permettant d'initialiser le plateau de jeu
 * @usefulfor interface graphique
 */
void init_interface(void) {
  // initialisation score à 0
  set_score_black(0);
  set_score_white(0);
  /**
   * Initilisation du damier (D4=blanc, E4=noir, D5=noir, E5=blanc)
   * D2 blanc équivaut à {3, 1, 1}
   * (car D, 4e lettre, donc => 3)
   * (4, 4e chiffre, donc => 1)
   */
  set_case(3, 3, WHITE);

  set_case(4, 3, BLACK);

  set_case(3, 4, BLACK);

  set_case(4, 4, WHITE);

  /**
   * Initialisation des scores et des joueurs
   */
  if (get_player_color() == WHITE) {
    set_label_black(LABEL_YOU);
    set_label_white(LABEL_ADVERSARY);
  } else {  // noir
    set_label_black(LABEL_ADVERSARY);
    set_label_white(LABEL_YOU);
  }
}

/**
 * Fonction reinitialisant la liste des joueurs sur l'interface graphique
 * @usefulfor interface graphique
 */
void reset_liste_joueurs(void) {
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter(
      GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(
          gtk_builder_get_object(p_builder, "textview_joueurs")))),
      &start);
  gtk_text_buffer_get_end_iter(
      GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(
          gtk_builder_get_object(p_builder, "textview_joueurs")))),
      &end);

  gtk_text_buffer_delete(
      GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(
          gtk_builder_get_object(p_builder, "textview_joueurs")))),
      &start, &end);
}

/**
 * Fonction permettant d'ajouter un joueur
 * dans la liste des joueurs sur l'interface graphique
 * @param login le login du joueur
 * @param adresse l'adresse ip du joueur
 * @param port le port du joueur
 * @usefulfor interface graphique
 */
void add_player_to_list(char *login, char *adresse, char *port_s) {
  const gchar *joueur;

  joueur = g_strconcat(login, " - ", adresse, " : ", port_s, "\n", NULL);

  gtk_text_buffer_insert_at_cursor(
      GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(
          gtk_builder_get_object(p_builder, "textview_joueurs")))),
      joueur, strlen(joueur));
}

// Big Boy fonction
// Itération sur l'ensemble des sockets
static void *f_com_socket(void *p_arg) {
  int i, nbytes;

  char buf[MAX_DATA_SIZE];

  sigset_t signal_mask;
  int fd_signal, rv;

  char *token;
  char *saveptr;

  // ajout initiasation requete
  request_s req;

  /* Association descripteur au signal SIGUSR1 */
  sigemptyset(&signal_mask);
  sigaddset(&signal_mask, SIGUSR1);

  if (sigprocmask(SIG_BLOCK, &signal_mask, NULL) == -1) {
    printf("[%d] Erreur sigprocmask\n", port);
    return 0;
  }

  fd_signal = signalfd(-1, &signal_mask, 0);

  if (fd_signal == -1) {
    printf("[%d] Erreur signalfd\n", port);
    return 0;
  }

  /*
   * Ajout descripteur du signal dans ensemble
   * de descripteur utilisé avec fonction select
   */
  FD_SET(fd_signal, &master);

  if (fd_signal > fdmax) {
    fdmax = fd_signal;
  }

  while (1) {
    read_fds = master;  // copie des ensembles

    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("Problème avec select");
      exit(4);
    }

    printf("[%d] Entrée dans boucle for\n", port);
    for (i = 0; i <= fdmax; ++i) {
      /*printf
         ("[%d] newsockfd=%d, iteration %d boucle for\n",
         port, newsockfd, i); */

      if (FD_ISSET(i, &read_fds)) {
        if (i == fd_signal) {
          printf("i = fd_signal\n");
          /*
           * Cas où de l'envoie du signal par l'interface graphique
           * pour connexion au joueur adverse
           */
          if (newsockfd == -1) {
            memset(&hints, 0, sizeof(hints));  // rempli hints de 0
            hints.ai_family =
                AF_UNSPEC;  //ça peut donc être une IPV4 ou IPV6 ; ça gère les 2
            hints.ai_socktype = SOCK_STREAM;  // communication connectée ;
                                              // Utilise le protocole TCP

            rv = getaddrinfo(adv_addr, adv_port, &hints,
                             &servinfo);  // rempli automatiquement une
                                          // structure addinfo (param4)

            if (rv != 0)  // getaddrinfo a-t-il fonctionné ? 0 = Non
            {
              fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(rv));
            }
            // création socket et attachement
            for (p = servinfo; p != NULL; p = p->ai_next) {
              if ((newsockfd =
                       socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
                  -1)  // sockfd est la socket du client
              {
                perror("client: socket");
                continue;
              }
              if (connect(newsockfd, p->ai_addr, p->ai_addrlen) ==
                  -1)  // attachement de la socket - client
              {
                close(newsockfd);
                // FD_CLR(newsockfd, &master);
                perror("client: connect");
                continue;
              }
              break;
            }
            if (p == NULL) {
              fprintf(stderr, "server : failed to bind\n");
              exit(2);
            }

            freeaddrinfo(servinfo);  // libère la structure

            /*
             * Ajout descripteur du signal dans ensemble
             * de descripteur utilisé avec fonction select
             */
            FD_SET(newsockfd, &master);

            if (newsockfd > fdmax) {
              fdmax = newsockfd;
            }

            close(sockfd);
            FD_CLR(sockfd, &master);

            // fermeture et suppression de fd_signal de l'ensemble master
            close(fd_signal);
            FD_CLR(fd_signal, &master);

            // Choix de couleur à compléter

            set_player_couleur(BLACK);
            req.type = htons((unsigned short)COLOR);
            req.color = htons((unsigned short)get_adv_color());

            bzero(buf, MAX_DATA_SIZE);

            printf("Requête à envoyer %u : %u\n", req.type, req.color);
            snprintf(buf, MAX_DATA_SIZE, "%u, %u", req.type, req.color);

            printf("Envoyer requête %s\n", buf);
            if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
              perror("send");
              close(newsockfd);
            }
            // le noir joue en 1er
            // initialisation interface graphique
            init_interface();
            unfreeze_board();
          }
        }

        if (i == sockfd) {  // acceptation connexion adversaire

          printf("i == sockfd\n");

          if (newsockfd == -1) {
            addr_size = sizeof(adv_sockaddr);
            /*
             * Accepte une connexion client
             * et retourne nouveau descripteur de socket de service
             * pour traiter la requête du client
             * Fonction bloquante
             * @param1 socket d'écoute côté serveur
             * @param2 structure adresse + port du client
             * @param3 taille du param2 (cf ci-dessus)
             */
            newsockfd = accept(sockfd, adv_sockaddr, (socklen_t *)&addr_size);

            if (newsockfd == -1) {
              perror("problème avec accept");
            } else {
              /*
               * Ajout du nouveau descripteur dans ensemble
               * de descripteur utilisé avec fonction select
               */
              FD_SET(newsockfd, &master);

              if (newsockfd > fdmax) {
                fdmax = newsockfd;
              }

              //

              // fermeture socket écoute car connexion déjà établie avec
              // adversaire
              printf(
                  "[Port joueur : %d] Bloc if du serveur, fermeture socket "
                  "écoute\n",
                  port);
              FD_CLR(sockfd, &master);
              close(sockfd);
            }

            // fermeture et suppression de FD_SIGNAL de l'ensemble master
            close(fd_signal);
            FD_CLR(fd_signal, &master);

            // Réception et traitement des messages du joueur adverse
            bzero(buf, MAX_DATA_SIZE);
            recv(newsockfd, buf, MAX_DATA_SIZE, 0);

            token = strtok_r(buf, ",", &saveptr);
            sscanf(token, "%hu", &(req.type));
            req.type = ntohs(req.type);
            printf("type message %u\n", req.type);

            if (req.type == COLOR) {
              token = strtok_r(NULL, ",", &saveptr);
              sscanf(token, "%hu", &(req.color));
              req.color = ntohs(req.color);
              printf("couleur %hu\n", req.color);
              set_player_couleur((color_e)req.color);
            }

            // Quand on reçoit les premières coordonnées

            init_interface();

            if (req.type == COORD) {
              unfreeze_board();
            }
            gtk_widget_set_sensitive(
                (GtkWidget *)gtk_builder_get_object(p_builder, "button_start"),
                FALSE);
          }
        } else {
          if (i == newsockfd) {
            // Réception et traitement des messages du joueur adverse
            bzero(buf, MAX_DATA_SIZE);
            nbytes = recv(newsockfd, buf, MAX_DATA_SIZE, 0);

            printf("reception de %d octets\n", nbytes);

            token = strtok_r(buf, ",", &saveptr);
            sscanf(token, "%hu", &(req.type));
            req.type = ntohs(req.type);
            printf("type message %u\n", req.type);

            if (req.type == COORD) {
              token = strtok_r(NULL, ",", &saveptr);
              sscanf(token, "%hu", &(req.x));
              req.x = ntohs(req.x);
              printf("x %hu\n", req.x);

              token = strtok_r(NULL, ",", &saveptr);
              sscanf(token, "%hu", &(req.y));
              req.y = ntohs(req.y);
              printf("y %hu\n", req.y);
              set_case(req.x, req.y, get_adv_color());
              update_cells(req.x, req.y, get_adv_color());
              unfreeze_board();
            } else if (req.type == NO_MOVE_POSSIBLE) {
              printf("adversaire n'a pas plus jouer\n");
              unfreeze_board();
            } else if (req.type == END) {
              printf("fin du jeu\n");
              unfreeze_board();
            }
          }
        }
      }
    }
  }

  return (NULL);
}

// Fonction dans laquelle je place l'ensemble
// des instructions necessaires à l'initialisation de l'interface GTK+
bool init_gtk(char title[MAX_TITLE_LENGTH]) {
  p_builder = gtk_builder_new();
  if (p_builder == NULL) {
    return false;
  }
  gtk_builder_add_from_file(p_builder, LAYOUT_FILE, &p_err);
  if (p_err != NULL) {
    return false;
  }

  /* Récupération d'un pointeur sur la fenêtre. */
  p_win = (GtkWidget *)gtk_builder_get_object(p_builder, "window1");

  gtk_window_set_title(GTK_WINDOW(p_win), title);

  // bindings des cases du plateau
  {
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH1"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH2"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH3"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH4"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH5"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH6"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH7"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
    g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH8"),
                     "button_press_event", G_CALLBACK(on_click_cell), NULL);
  }

  /* Gestion clic boutons interface */
  g_signal_connect(gtk_builder_get_object(p_builder, "button_connect"),
                   "clicked", G_CALLBACK(on_click_connect), NULL);
  g_signal_connect(gtk_builder_get_object(p_builder, "button_start"), "clicked",
                   G_CALLBACK(on_click_start), NULL);

  /* Gestion clic bouton fermeture fenêtre */
  g_signal_connect_swapped(G_OBJECT(p_win), "destroy",
                           G_CALLBACK(gtk_main_quit), NULL);

  freeze_board();  // on gèle le plateau afin d'empécher un clic sur un plateau
                   // vide hors partie
  gtk_widget_show_all(p_win);
  gtk_main();
  return true;
}

// Fonction dans laquelle je range l'initialistion
// de la socket serveur qui attend les demandes de connexion
bool init_socket(char *port_str) {
  memset(&hints, 0, sizeof(hints));  // idem que ci-dessus
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // trouve automatiquement mon IP
  // car premier param de getaddrinfo = NULL
  status = getaddrinfo(NULL, port_str, &hints, &servinfo);
  if (status != 0) {
    fprintf(stderr, "[%s] getaddrinfo error: %s\n", port_str,
            gai_strerror(status));
    return 1;
  }
  // Création socket et attachement
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("[server] socket");
      continue;
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) ==
        -1)  // attachement de la socket - serveur
    {
      close(sockfd);
      // FD_CLR(sockfd, &master);
      perror("[server] bind");
      continue;
    }
    printf("[%s] server socket bound successfully\n", port_str);
    break;
  }
  if (p == NULL) {
    fprintf(stderr, "[%s] server socket bind fail\n", port_str);
    return false;
  }
  freeaddrinfo(servinfo);  // Libère structure
  /*
   * le serveur ecoute et est en attente d'une demande de connexion
   * socket d'écoute cote serveur
   * Fonction bloquante
   * @param2 est nombre max de connexion en attente de traitement
   */
  listen(sockfd, 1);

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  /*
   * Ajout descripteur du signal dans ensemble
   * de descripteur utilisé avec fonction select
   */
  FD_SET(sockfd, &master);
  read_fds = master;  // copie des ensembles
  fdmax = sockfd;
  return true;
}

int main(int argc, char **argv) {
  int error_code;
  char title[MAX_TITLE_LENGTH];

  if (argc != 2) {
    printf("Usage :\n  %s port\n", argv[0]);
    exit(1);
  }

  port = atoi(argv[1]);

  if (!init_socket(argv[1])) {
    exit(1);
  }

  // On met le plateau à vide
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      board[i][j] = EMPTY;
    }
  }

  // On créer le thread dans lequel on va itérer sur notre ensemble de socket
  // La boucle des échanges du jeu s'éxécutera dans la fonction f_com_socket
  error_code = pthread_create(&thread_id, NULL, f_com_socket, NULL);
  if (error_code != 0) {
    fprintf(stderr, "%s\n", strerror(error_code));
    exit(1);
  }

  gtk_init(&argc, &argv);
  snprintf(title, MAX_TITLE_LENGTH, "Jeu du Othello - %d", port);
  if (!init_gtk(title)) {
    g_error("[%d] %s", port, p_err->message);
    g_error_free(p_err);
  }

  return EXIT_SUCCESS;
}
