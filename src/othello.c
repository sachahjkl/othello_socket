
#define _GNU_SOURCE
#include "othello.h"

#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>

int port;                   // numero port passe a l'appel
char *adv_addr, *adv_port;  // Info sur adversaire

int sockfd, newsockfd = -1;     // descripteurs de socket
int addr_size;                  // taille adresse
struct sockaddr *adv_sockaddr;  // structure pour stocker adresse adversaire

fd_set master, read_fds, write_fds;  // ensemble de socket pour toutes les
                                     // sockets actives avec select
int fdmax;                           // utilise pour select

struct addrinfo hints, *servinfo, *p;

pthread_t thread_id;  // Id du thread dans lequel tourne la boucle du jeu

// Globales de l'interface GTK+
GtkBuilder *p_builder = NULL;
GError *p_err = NULL;
GtkWidget *p_win = NULL;

const char *get_type_str(type_e type) {
  switch (type) {
    case COORD:
      return "COORD";
    case COLOR:
      return "COLOR";
    case END:
      return "END";
    case NO_MOVE_POSSIBLE:
      return "NO_MOVE_POSSIBLE";
    default:
      return "UNKNOWN";
  }
}

const char *get_color_str(color_e color) {
  switch (color) {
    case BLACK:
      return "BLACK";
    case WHITE:
      return "WHITE";
    case OUT:
      return "OUT";
    case EMPTY:
      return "EMPTY";
    default:
      return "UNKNOWN";
  }
}

void coord_to_indexes(const gchar *coord, int *col, int *row) {
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

  *row = atoi(coord + 1) - 1;
}

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

void set_label_black(const char *texte) {
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J1")),
                     texte);
}

void set_label_white(const char *texte) {
  gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J2")),
                     texte);
}

void set_score_black(int score) {
  char *s;

  s = malloc(5 * sizeof(char));
  sprintf(s, "%d", score);

  gtk_label_set_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ1")), s);
}

int get_score_black(void) {
  const gchar *c;

  c = gtk_label_get_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ1")));

  return atoi(c);
}

void set_score_white(int score) {
  char *s;

  s = malloc(5 * sizeof(char));
  sprintf(s, "%d", score);

  gtk_label_set_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ2")), s);
}

int get_score_white(void) {
  const gchar *c;

  c = gtk_label_get_text(
      GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ2")));

  return atoi(c);
}

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

void set_case(int col, int row, color_e p_color) {
  //
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
  // On insere la case de la couleur du joueur qui joue
  printf("[%d] cell {x: %d, y: %d} goes from %d to %d\n", port, col, row,
         board[col][row], p_color);
  board[col][row] = p_color;
  // MAJ de l'image vers la couleur du joueur qui joue
  update_cell_image(col, row, p_color);
}

bool change_ligne(int col, int row, int dx, int dy, color_e p_color) {
  int cell_count = 0;
  // tant qu'on parcout une case de couleur de celle opposée que
  // l'on veut jouer
  col += dx;
  row += dy;
  while (get_board_color(col, row) == get_opposite_color(p_color)) {
    // On augmente le nombre de cases parcourue
    cell_count++;
    // On incrémente dans la direction choisie
    col += dx;
    row += dy;
  }
  // Si tombe sur une case de notre couleur et qu'on a parcouru au moins
  // 1 case de couleur adverse, on dit LETS GOO !
  return ((get_board_color(col, row) == p_color) && (cell_count > 0));
}

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

void update_cell_directed(int col, int row, int dx, int dy, color_e p_color) {
  col += dx;
  row += dy;
  while (get_board_color(col, row) == get_opposite_color(p_color)) {
    // Tant qu'on pulv la couleur adverse
    set_case(col, row, p_color);
    // On incrémente dans la direction choisie
    col += dx;
    row += dy;
  }
}

bool game_is_over(void) {
  int black_score, white_score, max_score;
  black_score = get_score_black();
  white_score = get_score_white();
  max_score = BOARD_SIZE * BOARD_SIZE;
  return ((black_score + white_score) == max_score);
}

bool stalemate(void) {
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

bool player_cant_play(void) {
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

color_e get_board_color(int col, int row) {
  if ((col < 0) || (row < 0) || (col >= BOARD_SIZE) || (row >= BOARD_SIZE)) {
    return OUT;
  } else {
    return board[col][row];
  }
}

color_e get_player_color(void) { return my_color; }

void set_player_couleur(color_e p_color) { my_color = p_color; }

color_e get_adv_color(void) {
  if (get_player_color() == WHITE) {
    return BLACK;
  }
  if (get_player_color() == BLACK) {
    return WHITE;
  }
  return EMPTY;
}

color_e get_opposite_color(color_e p_color) {
  if (p_color == WHITE) {
    return BLACK;
  }
  if (p_color == BLACK) {
    return WHITE;
  }
  return EMPTY;
}

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
    // Partie finie car plateau plein
    freeze_board();

    // préparation message et envoie sur socket à adversaire
    req.type = htons((unsigned short)END);

    printf("[%d] request type : %s\n", port, get_type_str(ntohs(req.type)));
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%hu", req.type);

    printf("[%d] sending request %s\n", port, buf);
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_lose_or_win();
  } else if (stalemate()) {
    // Partie fini car les joueurs ne peuvent plus poser de pions
    int empty_cells =
        (BOARD_SIZE * BOARD_SIZE) - (get_score_black() + get_score_white());
    if (get_score_black() > get_score_white()) {
      set_score_black(get_score_black() + empty_cells);
    } else {
      set_score_white(get_score_white() + empty_cells);
    }

    // On dit que c'est finito
    req.type = htons((unsigned short)END);

    printf("[%d] request type : %s\n", port, get_type_str(ntohs(req.type)));
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%hu", req.type);

    printf("[%d] sending request %s\n", port, buf);
    // Et zé parti
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_lose_or_win();
  } else if (player_cant_play()) {
    // le joueur ne peut pas placer le pion mais l'autre si

    freeze_board();

    // On dit que je peux pas jouer azy j'ai le démon
    req.type = htons((unsigned short)NO_MOVE_POSSIBLE);

    printf("[%d] request type : %s\n", port, get_type_str(ntohs(req.type)));
    bzero(buf, MAX_DATA_SIZE);
    snprintf(buf, MAX_DATA_SIZE, "%hu", req.type);

    printf("[%d] sending request %s\n", port, buf);
    if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
      perror("send");
      close(newsockfd);
    }

    show_stalemate();
  } else {
    if (playable(col, row, get_player_color())) {
      // Dans cette situation on peut jouer la case :)
      // changer la case à la couleur
      set_case(col, row, get_player_color());
      freeze_board();
      // MAJ Cellules et scores
      update_cells(col, row, get_player_color());

      printf("[%d] have to send coords {x: %d, y: %d}\n", port, col, row);

      // on envoie notre move à l'adversaire
      req.type = htons((unsigned short)COORD);
      req.x = htons((unsigned short)col);
      req.y = htons((unsigned short)row);
      printf("[%d] sending request {type: %s, x: %hu, y: %hu}\n", port,
             get_type_str(ntohs(req.type)), ntohs(req.x), ntohs(req.y));
      bzero(buf, MAX_DATA_SIZE);
      snprintf(buf, MAX_DATA_SIZE, "%hu%s%hu%s%hu", req.type, SEPARATOR, req.x,
               SEPARATOR, req.y);

      printf("[%d] sending formatted request %s\n", port, buf);
      if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
        perror("send");
        close(newsockfd);
      }
    }
  }
}

char *read_adv_server(void) {
  GtkWidget *entry_addr_srv;

  entry_addr_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_adr");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

char *read_server_port(void) {
  GtkWidget *entry_port_srv;

  entry_port_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

char *read_login(void) {
  GtkWidget *entry_login;

  entry_login = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_login");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_login));
}

char *read_adv_addr(void) {
  GtkWidget *entry_addr_adv;

  entry_addr_adv =
      (GtkWidget *)gtk_builder_get_object(p_builder, "entry_addr_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_adv));
}

char *read_adv_port(void) {
  GtkWidget *entry_port_adv;

  entry_port_adv =
      (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_adv));
}

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

void show_win(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_YOU_WON);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

void show_lost(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_YOU_LOST);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

void show_stalemate(void) {
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, MESSAGE_STALEMATE);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

static void on_click_connect(GtkWidget *b) {
  // TODO -> Pas eu le temps & que 2 dans le groupe
}

void disable_button_start(void) {
  gtk_widget_set_sensitive(
      (GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);
}

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

void init_interface(void) {
  // initialisation des scores à 0
  set_score_black(0);
  set_score_white(0);

  // On initialise les cases initiales du plateau
  set_case(3, 3, WHITE);
  set_case(4, 3, BLACK);
  set_case(3, 4, BLACK);
  set_case(4, 4, WHITE);

  if (get_player_color() == WHITE) {
    set_label_black(LABEL_YOU);
    set_label_white(LABEL_ADVERSARY);
  } else {  // noir
    set_label_black(LABEL_ADVERSARY);
    set_label_white(LABEL_YOU);
  }
}

// Pas utilisée
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

// Pas utilisée
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
  int i;

  char buf[MAX_DATA_SIZE];

  sigset_t signal_mask;
  int fd_signal, rv;

  char *token;
  char *tokenptr;

  // Structure dans laquelle on range les données des échanges
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
      perror("select");
      exit(4);
    }

    printf("[%d] Start iterating over sockets\n", port);
    for (i = 0; i <= fdmax; ++i) {
      /*printf
         ("[%d] newsockfd=%d, iteration %d boucle for\n",
         port, newsockfd, i); */

      if (FD_ISSET(i, &read_fds)) {
        if (i == fd_signal) {
          /*
           * Cas où de l'envoie du signal par l'interface graphique
           * pour connexion au joueur adverse
           */
          if (newsockfd == -1) {
            memset(&hints, 0, sizeof(hints));  // on met à 0 la structure
            hints.ai_family = AF_UNSPEC;       // IPV4/IPV6
            hints.ai_socktype = SOCK_STREAM;   // Socket TCP

            rv = getaddrinfo(adv_addr, adv_port, &hints, &servinfo);

            if (rv != 0)  // Si on a pas récupéré les infos
            {
              fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(rv));
            }
            //
            for (p = servinfo; p != NULL; p = p->ai_next) {
              if ((newsockfd = socket(p->ai_family, p->ai_socktype,
                                      p->ai_protocol)) == -1) {
                perror("socket");
                continue;
              }
              if (connect(newsockfd, p->ai_addr, p->ai_addrlen) ==
                  -1)  // connexion à la socket client
              {
                close(newsockfd);
                perror("connect");
                continue;
              }
              break;
            }
            if (p == NULL) {
              fprintf(stderr, "[%d] failed to bind\n", port);
              exit(2);
            }

            freeaddrinfo(servinfo);

            // On rajoute à notre ensemble de socket
            FD_SET(newsockfd, &master);

            if (newsockfd > fdmax) {
              fdmax = newsockfd;
            }

            close(sockfd);
            FD_CLR(sockfd, &master);

            // On a plus besoin du bouton de lancement de partie
            close(fd_signal);
            FD_CLR(fd_signal, &master);

            // Dans les règles du Othello, le joueur noir commence
            set_player_couleur(BLACK);
            req.type = htons((unsigned short)COLOR);
            req.color = htons((unsigned short)get_adv_color());

            bzero(buf, MAX_DATA_SIZE);
            printf("[%d] request {type: %s, color: %s}\n", port,
                   get_type_str(ntohs(req.type)),
                   get_color_str(ntohs(req.color)));
            snprintf(buf, MAX_DATA_SIZE, "%hu%s%hu", req.type, SEPARATOR,
                     req.color);

            printf("[%d] sending formatted request : %s\n", port, buf);
            if (send(newsockfd, &buf, strlen(buf), 0) == -1) {
              perror("send");
              close(newsockfd);
            }
            init_interface();
            // On peut dégeler puisqu'on est les premiers à jouer
            unfreeze_board();
          }
        }

        if (i == sockfd) {  // Adversaire veut se connecter

          if (newsockfd == -1) {
            addr_size = sizeof(adv_sockaddr);
            // On initialise la socket pour communiquer avec l'adversaire
            newsockfd = accept(sockfd, adv_sockaddr, (socklen_t *)&addr_size);

            if (newsockfd == -1) {
              perror("accept");
            } else {
              // Si tout va bien, on peut l'ajouter à notre ensemble
              // de socket.
              FD_SET(newsockfd, &master);

              if (newsockfd > fdmax) {
                fdmax = newsockfd;
              }

              // On peut fermer la socket d'écoute puisqu'on vient d'accepter
              // la communication avec un adversaire
              printf("[%d] closing listening socket\n", port);
              FD_CLR(sockfd, &master);
              close(sockfd);
            }

            // On a plus besoin de vérifier si on clique sur "Démarrer Partie"
            close(fd_signal);
            FD_CLR(fd_signal, &master);

            bzero(buf, MAX_DATA_SIZE);
            recv(newsockfd, buf, MAX_DATA_SIZE, 0);

            token = strtok_r(buf, SEPARATOR, &tokenptr);
            sscanf(token, "%hu", &(req.type));
            req.type = ntohs(req.type);
            printf("[%d] received type: %s\n", port, get_type_str(req.type));

            if (req.type == COLOR) {
              token = strtok_r(NULL, SEPARATOR, &tokenptr);
              sscanf(token, "%hu", &(req.color));
              req.color = ntohs(req.color);
              printf("[%d] received color:  %s\n", port,
                     get_color_str(req.color));
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
            // On traite les infos provenant de l'adversaire
            bzero(buf, MAX_DATA_SIZE);
            recv(newsockfd, buf, MAX_DATA_SIZE, 0);

            token = strtok_r(buf, SEPARATOR, &tokenptr);
            sscanf(token, "%hu", &(req.type));
            req.type = ntohs(req.type);
            printf("[%d] received type: %s\n", port, get_type_str(req.type));

            if (req.type == COORD) {
              token = strtok_r(NULL, SEPARATOR, &tokenptr);
              sscanf(token, "%hu", &(req.x));
              req.x = ntohs(req.x);

              token = strtok_r(NULL, SEPARATOR, &tokenptr);
              sscanf(token, "%hu", &(req.y));
              req.y = ntohs(req.y);
              printf("[%d] received coords {x: %hu y: %hu}\n", port, req.x,
                     req.y);
              set_case(req.x, req.y, get_adv_color());
              update_cells(req.x, req.y, get_adv_color());
              unfreeze_board();
            } else if (req.type == NO_MOVE_POSSIBLE) {
              printf("[%d] adversary had no moves\n", port);
              unfreeze_board();
            } else if (req.type == END) {
              printf("[%d] GAME OVER\n", port);
              unfreeze_board();
            }
          }
        }
      }
    }
  }

  return NULL;
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
  int status;
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

// Point d'entrée du programme
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
