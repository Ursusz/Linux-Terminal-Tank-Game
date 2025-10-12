#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define ROWS 20
#define COLUMNS 20
// numele segmentului de memorie partajata
#define SHM_NAME "/shm_tabla"
#define MATRIX_SIZE (ROWS * COLUMNS)
#define MAX_BULLETS 100

typedef struct{
  int x, y;
} player_pos;

typedef struct{
  int direction;
  int x, y;
} bullet;

typedef struct{
  sem_t cell_semaphores[MATRIX_SIZE];
  char tabla[MATRIX_SIZE];
  
  player_pos players[2];
  char plyr1, plyr2;
  int plyr1HP, plyer2HP;
  bool creator_quit;

  bullet bullets[MAX_BULLETS];
  int num_bullets;
} shared_matrix_t;