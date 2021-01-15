#ifndef GAME_LOGIC
#define GAME_LOGIC

#define MAX_BLACK 32
#define MAX_WHITE 32

#define BOARD_SIZE 8

const enum color_e { BLACK, WHITE, EMPTY };

const enum color_e FIRST_PLAYER = BLACK;  // 1 : noir ; 0 : blanc

struct coord_s {
  const int x;
  const int y;
};

struct move_s {
  const enum color_e color;
  const struct coord_s from, to;
};

struct score_s {
  int black;
  int white;
} score = {0, 0};

enum color_e board[BOARD_SIZE][BOARD_SIZE] = {
    {EMPTY},
    {EMPTY},
    {EMPTY},
    {EMPTY, EMPTY, EMPTY, WHITE, BLACK, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, BLACK, WHITE, EMPTY, EMPTY, EMPTY},
    {EMPTY},
    {EMPTY},
    {EMPTY}};

#endif