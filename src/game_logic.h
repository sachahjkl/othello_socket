#ifndef GAME_LOGIC
#define GAME_LOGIC

#define MAX_BLACK 32
#define MAX_WHITE 32

#define BOARD_SIZE 8


static enum colors {
    BLACK,
    WHITE,
    EMPTY
};

const enum colors FIRST_PLAYER = BLACK; // 1 : noir ; 0 : blanc

static struct coord
{
    const int x;
    const int y;
};

static struct move
{
    const enum colors color;
    const struct coord from, to;
};

static enum colors board[BOARD_SIZE][BOARD_SIZE] = {{EMPTY}};

#endif