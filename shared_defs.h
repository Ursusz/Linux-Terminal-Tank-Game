#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdbool.h>

#define ROWS 20
#define COLUMNS 65
// numele segmentului de memorie partajata
#define SHM_NAME "/shm_tabla"
#define MATRIX_SIZE (ROWS * COLUMNS)
#define MAX_BULLETS 2

typedef struct{
  int x, y;
} player_pos;

typedef struct{
  int direction;
  int x, y;
  bool available;
} bullet;

typedef struct{
  sem_t cell_semaphores[MATRIX_SIZE];
  char tabla[MATRIX_SIZE];
  
  player_pos players[2];
  char plyr1, plyr2;
  int plyr1HP, plyr2HP;
  bool creator_quit;
  bool both_players_online;

  bullet bullets[MAX_BULLETS];
} shared_matrix_t;