#include "game_logic.h"

#include <stdlib.h>

void init_game(game_s *game, color_e first_player) {
  game->turn = first_player;
  game->board[3][3] = WHITE;
  game->board[4][4] = WHITE;
  game->board[3][4] = BLACK;
  game->board[4][3] = BLACK;
  game->score.black = 0;
  game->score.white = 0;
}

int is_over(game_s *game) {
  const score_s score = game->score;
  const int score_max = BOARD_SIZE * BOARD_SIZE;
  return ((score.black + score.white) == score_max);
}

static int is_valid(game_s *game, coord_s move) {
  const int x = move.x;
  const int y = move.y;
  if (is_out(move)) {
    return EXIT_FAILURE;
  }
  if (game->board[x][y] != EMPTY) {
    return EXIT_FAILURE;
  }

  if (!(check_move_line(game, move, (coord_s){1, 1}) ||
        check_move_line(game, move, (coord_s){-1, -1}) ||
        check_move_line(game, move, (coord_s){0, -1}) ||
        check_move_line(game, move, (coord_s){0, 1}) ||
        check_move_line(game, move, (coord_s){1, 0}) ||
        check_move_line(game, move, (coord_s){-1, 0}) ||
        check_move_line(game, move, (coord_s){1, -1}) ||
        check_move_line(game, move, (coord_s){-1, 1}))) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// static

static int is_out(coord_s move) {
  const int x = move.x;
  const int y = move.y;
  return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

static int check_move_line(game_s *game, coord_s move, coord_s differential) {
  int count = 0;
  int x = move.x;
  int y = move.y;
  const int dx = differential.x;
  const int dy = differential.y;
  // on fait un parcours tant que la couleur de la selection de direction
  // est différente
  x += dx;
  y += dy;
  while (get_color(game, (coord_s){x, y}) == get_adversary_color(game)) {
    // on increment le compteur
    count++;
    // on increment le parcours vers la direction choisie
    x += dx;
    y += dy;
  }
  // si la couleur qu'on vient de parcourir est la même que la couleur du joueur
  // qui vient de jouer ET que le compteur est supéreur a 1(pour dire que c'est
  // pas un voisin de la meme couleur)
  return get_color(game, (coord_s){x, y}) == game->turn && count > 0;
}

static color_e get_color(game_s *game, coord_s coordonates) {
  const x = coordonates.x;
  const y = coordonates.y;
  if (is_out(coordonates)) {
    return OUT;
  } else {
    return (game->board[x][y]);
  }
}

static color_e get_adversary_color(game_s *game) {
  if (game->turn == BLACK) {
    return WHITE;
  } else {
    return BLACK;
  }
}
