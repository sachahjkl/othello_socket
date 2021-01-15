#ifndef SERVER_H
#define SERVER_H

#include "game_logic.h"
// static const r_socket;
// static const x_socket;

// int init_socket(int port);

// int connect_socket(char* ip, int port);

#define REQUEST_SEPARATOR '|'

struct state_s {
  enum color_e color;
  struct coord_s coord;
};

#endif