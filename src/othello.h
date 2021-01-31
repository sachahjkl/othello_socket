#ifndef OTHELLO_H
#define OTHELLO_H

#include <gtk/gtk.h>
#include <stdbool.h>

#define MAX_DATA_SIZE 256
#define MAX_TITLE_LENGTH 45

#define BOARD_SIZE 8

GtkWidget *p_win;  // fenêtre

typedef enum color { BLACK, WHITE, EMPTY, OUT } color_e;

const color_e DEFAULT_FIRST_PLAYER = BLACK;  // 1 : noir ; 0 : blanc

color_e board[BOARD_SIZE][BOARD_SIZE];  // damier de pions

/**
 * Fonction permettant de changer l'image d'une case du damier
 * (indiqué par sa colonne et sa ligne)
 * @param col la colonne de la case du damier
 * @param lig la ligne de la case du damier
 * @param couleur_j la couleur du joueur (0 noir, 1 blanc)
 */
void change_img_case(int col, int lig, color_e couleur_j);

/**
 * Fonction permettant changer nom joueur blanc dans cadre Score
 * @param texte nom du J1 (blanc)
 * @usefulfor interface graphique
 */
void set_label_J1(char *texte);

/**
 * Fonction permettant changer nom joueur noir dans cadre Score
 * @param texte nom du J2 (noir)
 * @usefulfor interface graphique
 */
void set_label_J2(char *texte);

/**
 * Fonction permettant de changer score joueur blanc dans cadre Score
 * @param score le score du J1 (blanc)
 * @see get_score_J1(void)
 */
void set_score_J1(int score);

/**
 * Fonction permettant de récupérer score joueur blanc dans cadre Score
 * @return le score du J1 (blanc)
 * @see set_score_J1(int)
 */
int get_score_J1(void);

/**
 * Fonction permettant de changer score joueur noir dans cadre Score
 * @param score le score du J2 (noir)
 * @see #get_score_J2(void)
 */
void set_score_J2(int score);

/**
 * Fonction permettant de récupérer score joueur noir dans cadre Score
 * @return le score du J2 (noir)
 * @see #set_score_J2(int)
 */
int get_score_J2(void);

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
static void coup_joueur(GtkWidget *p_case);

/**
 * Fonction retournant texte du champs adresse
 * du serveur de l'interface graphique
 * @return l'adresse du serveur
 */
char *lecture_addr_serveur(void);

/**
 * Fonction retournant le texte du champs port
 * du serveur de l'interface graphique
 * @return le port du serveur
 */
char *lecture_port_serveur(void);

/**
 * Fonction retournant le texte du champs login
 * de l'interface graphique
 * @return le login
 */
char *lecture_login(void);

/**
 * Fonction retournant le texte du champs adresse
 * du cadre Joueurs de l'interface graphique
 * @return l'adresse de l'adversaire
 */
char *lecture_addr_adversaire(void);

/**
 * Fonction retournant le texte du champs port
 * du cadre Joueurs de l'interface graphique
 * @return le port de l'adversaire
 */
char *lecture_port_adversaire(void);

/**
 * Fonction permettant d'afficher
 * soit #affiche_fenetre_gagne(void)
 * soit #affiche_fenetre_perdu(void)
 * en fonction des cas et en fonction
 * de la couleur qui jouer
 */
void affiche_fenetre(void);

/**
 * Fonction affichant une boite de dialogue
 * si la partie est gagnée
 * @usefulfor interface graphique
 */
void affiche_fenetre_gagne(void);

/**
 * Fonction affichant une boite de dialogue
 * si la partie est perdue
 * @usefulfor interface graphique
 */
void affiche_fenetre_perdu(void);

/**
 * Fonction affichant une boite de dialogue
 * si le joueur ne peut pas jouer
 * @usefulfor interface graphique
 */
void affiche_fenetre_impossible_jouer(void);

/**
 * Fonction appelée lors du clic du bouton Se connecter
 * @usefulfor interface graphique
 */
static void clique_connect_serveur(GtkWidget *b);

/**
 * Fonction desactivant bouton demarrer partie
 * @usefulfor interface graphique
 */
void disable_button_start(void);

/**
 * Fonction appelee lors du clic du bouton Demarrer partie
 * @usefulfor interface graphique
 */
static void clique_connect_adversaire(GtkWidget *b);

/**
 * Fonction desactivant les cases du damier
 * pour attendre que l'utilisateur adverse joue (ignore les clics du joueurs
 * courant)
 * @usefulfor interface graphique
 * @see #degele_damier(void)
 */
void gele_damier(void);

/**
 * Fonction activant les cases du damier
 * pour que l'utilisateur puisse jouer
 * @usefulfor interface graphique
 * @see #gele_damier(void)
 */
void degele_damier(void);

/**
 * Fonction permettant d'initialiser le plateau de jeu
 * @usefulfor interface graphique
 */
void init_interface_jeu(void);

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
void affich_joueur(char *login, char *adresse, char *port);

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
bool jouable(int col, int lig, int couleur_joueur);

/**
 * Fonction permettant de changer la couleur de la case
 */
void set_case(int colonne, int ligne, color_e couleur);

/**
 * Fonction qui retourne si on doit changer la ligne
 */
bool change_ligne(int colonne, int ligne, int dx, int dy, color_e couleur);

/**
 * Fonction changeant la couleur des cases
 * @see #changement_case(int, int, int, int, int)
 */
void changements_case(int colonne, int ligne, int couleur);

/**
 * Fonction changeant la couleur des cases dans la direction choisie
 */
void changement_case(int colonne, int ligne, int chmtx, int chmty, int couleur);

/**
 * Fonction permettant de savoir
 * si on peut encore placer des pièces
 */
bool partie_finie();

/**
 * Fonction permettant de savoir
 * si les deux joueurs ne peuvent poser
 * plus de pièces
 * @see http://www.ffothello.org/images/jeu/regles/figure-10.jpg
 */
bool fin_prematuree();

/**
 * Fonction permettant de savoir
 * si le joueur ne peut pas jouer
 */
bool impossible_jouer();

/**
 * Fonction permettant de retourner la couleur de la case
 */
int return_couleur_case(int colonne, int ligne);

/**
 * Fonction retournant la couleur du joueur courant
 */
int get_couleur();

/**
 * Fonction initialisant la couleur du joueur courant
 */
void set_couleur(int couleur_joueur);

/**
 * Fonction retournant la couleur de l'adversaire
 */
int get_couleur_adversaire();

/**
 *   Fonction retournant la couleur adverse
 *  @param couleur notre couleur
 *  @return couleur adverse
 */
color_e get_couleur_adverse(color_e couleur);

#endif