#include <ncurses.h>
#include "shared_defs.h"

void print_board(char* tabla, int x1, int y1, int x2, int y2, char plyr1, char plyr2, bullet* bullets, int num_bullets, int plyr1HP, int plyr2HP){
  curs_set(0);
  int row = 0;
  int col = 10;
  
  clear();

  mvprintw(0, 0, "Player 1 (%c): %d HP", plyr1, plyr1HP);
  mvprintw(0, COLS - 20, "Player 2 (%c): %d HP", plyr2, plyr2HP);

  for (int i = 0; i < MATRIX_SIZE; i++){
    int rand_curent = (int)(i/ROWS);
    int coloana_curenta = i % COLUMNS;
    int rand = row + rand_curent + 2;
    int coloana = col + coloana_curenta;
    if (x1 == rand_curent && y1 == coloana_curenta){
      mvaddch(rand, coloana, plyr1);
    }
    if (x2 == rand_curent && y2 == coloana_curenta){
      mvaddch(rand, coloana, plyr2);
    }
    else{
      mvaddch(rand, coloana, tabla[i]);
    }
    for(int i = 0; i < num_bullets; i++){
      if(bullets[i].x == rand_curent && bullets[i].y == coloana_curenta){
        mvaddch(rand, coloana, '.');
      }
    }
    refresh();
  }
  addch('\n');
  refresh();
}