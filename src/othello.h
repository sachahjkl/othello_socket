#ifndef OTHELLO_H
#define OTHELLO_H

#include <gtk/gtk.h>
#include <stdbool.h>

#define MAX_DATA_SIZE 256
#define BOARD_SIZE 8
#define MAX_TITLE_LENGTH 45

#define LABEL_YOU "Vous"
#define LABEL_ADVERSARY "Adversaire"
#define MESSAGE_YOU_WON "Fin de la partie.\n\n Vous avez gagné!!!"
#define MESSAGE_YOU_LOST "Fin de la partie.\n\nVous avez perdu!"
#define MESSAGE_STALEMATE "Impossible de jouer.\n\n :)"

#define IMG_BLACK_TOKEN "UI_Glade/case_noir.png"
#define IMG_WHITE_TOKEN "UI_Glade/case_blanc.png"
#define LAYOUT_FILE "UI_Glade/Othello.glade"

typedef enum color { BLACK, WHITE, EMPTY, OUT } color_e;
typedef enum type { COORD, COLOR, END, NO_MOVE_POSSIBLE } type_e;

static color_e board[BOARD_SIZE]
                    [BOARD_SIZE];  // tableau associé au plateau de jeu
static color_e my_color;           // couleur de notre joueur

typedef struct request_data {
  unsigned short type;  // 0 pour la couleur, 1 pour les coordonnées x, y
  unsigned short x;
  unsigned short y;
  unsigned short color;
} request_s;

// Fonction permettant de changer l'image d'une case du damier
// (indiqué par sa colonne et sa ligne)
void update_cell_image(int col, int row, color_e p_color);

// Fonction permettant changer nom joueur blanc dans cadre Score
void set_label_black(const char *text);

// Fonction permettant changer nom joueur noir dans cadre Score
void set_label_white(const char *text);

// Fonction permettant de changer score joueur blanc dans cadre Score
void set_score_black(int score);

// Fonction permettant de récupérer score joueur blanc dans cadre Score
int get_score_black(void);

// Fonction permettant de changer score joueur noir dans cadre Score
void set_score_white(int score);

// Fonction permettant de récupérer score joueur noir dans cadre Score
int get_score_white(void);

// Fonction transformant les coordonnees du damier graphique
// en indexes pour matrice du damier
void coord_to_indexes(const gchar *coord, int *col, int *row);

// Fonction transformant les indexes pour matrice
// du damier en coordonnees du damier graphique
void indexes_to_coord(int col, int lig, char *coord);

// Appellé lors du clic sur une cellule
static void on_click_cell(GtkWidget *p_case);

// Retourne texte du champs adresse
// du serveur de l'interface graphique
char *read_adv_server(void);

// Retourne le texte du champs port
// du serveur de l'interface graphique
char *read_server_port(void);

// Retourne le texte du champs login
// de l'interface graphique
char *read_login(void);

// Retourne le texte du champs adresse
// du cadre Joueurs de l'interface graphique
char *read_adv_addr(void);

// Retourne le texte du champs port
// du cadre Joueurs de l'interface graphique
char *read_adv_port(void);

// Affiche selon le score si le joueur a gagné ou perdu
void show_lose_or_win(void);

// Affiche une boite de dialogue
// quand la partie est gagnée
void show_win(void);

// Affiche une boite de dialogue
// quand la partie est perdue
void show_lost(void);

// Affiche une boite de dialogue
// quand il n'y a plus de coups possibles
void show_stalemate(void);

// Appellée quand clic du bouton "Se connecter"
static void on_click_connect(GtkWidget *b);

// Désactive bouton demarrer partie
void disable_button_start(void);

// Fonction appelee lors du clic du bouton "Démarrer Partie"
static void on_click_start(GtkWidget *b);

// Fonction desactivant les cases du damier
// pour attendre que l'utilisateur adverse joue (ignore les clics du joueurs
// courant)
void freeze_board(void);

// Fonction activant les cases du damier
// pour que l'utilisateur puisse jouer
void unfreeze_board(void);

// Fonction permettant d'initialiser le plateau de jeu
void init_interface(void);

// Fonction reinitialisant la liste des joueurs sur l'interface graphique
void reset_liste_joueurs(void);

// Fonction permettant d'ajouter un joueur
// dans la liste des joueurs sur l'interface graphique
void add_player_to_list(char *login, char *adress, char *port_s);

// Fonction exécutée par le thread gérant
// les communications à travers la socket
static void *f_com_socket(void *p_arg);

// Ajout des entetes qui ne sont pas du prof

// Fonction vérifiant si une case est jouable
// 1/ en vérifiant qu'on ne depasse pas la taille du damier
// 2/ que l'on joue en se basant sur son voisin
bool playable(int col, int row, color_e p_color);

// Fonction permettant de changer la couleur de la case
void set_case(int col, int row, color_e p_color);

// Fonction qui retourne si on doit changer la row
bool change_ligne(int col, int row, int dx, int dy, color_e p_color);

// Fonction changeant la couleur des cases
void update_cells(int col, int row, color_e p_color);

// Fonction changeant la couleur des cases dans la direction choisie
void update_cell_directed(int col, int row, int dx, int dy, color_e p_color);

// Fonction permettant de savoir
// si on peut encore placer des pièces
bool game_is_over(void);

// Fonction permettant de savoir
// si les deux joueurs ne peuvent poser
// plus de pièces
bool stalemate(void);

// Fonction permettant de savoir
// si le joueur ne peut pas jouer
bool player_cant_play(void);

// Fonction permettant de retourner la couleur de la case
color_e get_board_color(int col, int row);

// Retourne la couleur du joueur courant
color_e get_player_color(void);

// Initialise la couleur du joueur courant
void set_player_couleur(color_e p_color);

// Retourne la couleur de l'adversaire
color_e get_adv_color(void);

// Retourne la couleur adverse
color_e get_opposite_color(color_e p_color);

#endif