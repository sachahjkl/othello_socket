#ifndef GAME_LOGIC
#define GAME_LOGIC

#define MAX_BLACK 32
#define MAX_WHITE 32

#define BOARD_SIZE 8

typedef enum color { BLACK, WHITE, EMPTY, OUT } color_e;

const color_e DEFAULT_FIRST_PLAYER = BLACK;  // 1 : noir ; 0 : blanc

typedef struct coord {
  const int x;
  const int y;
} coord_s;

typedef struct score {
  int black;
  int white;
} score_s;

typedef struct game {
  score_s score;
  color_e board[BOARD_SIZE][BOARD_SIZE];
  color_e turn;
} game_s;

void init_game(game_s *game, color_e first_player);

// Fonction permettant de savoir si la partie est terminée
int is_over(game_s *game);

int play_move(game_s *game, coord_s move);

static int is_valid(game_s *game, coord_s move);

static int is_out(coord_s move);

static color_e get_color(game_s *game, coord_s coordonates);

static color_e get_adversary_color(game_s *game);

#endif