#include <ncurses.h>
#include "shared_defs.h"

void print_board(char* tabla){
  curs_set(0);
  int row = 5;
  int col = 10;

  for (int i = 0; i < MATRIX_SIZE; i++){
    int rand = row + (int)(i/ROWS);
    int coloana = col + i % COLUMNS;
    mvaddch(rand, coloana, tabla[i]);
    refresh();
  }
  addch('\n');
}