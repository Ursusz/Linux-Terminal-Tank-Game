#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdbool.h>
#include <pthread.h>

#define ROWS 20
#define COLUMNS 65
#define SHM_NAME "/shm_tabla"
#define INIT_SEM_NAME "/tank_init_sem"
#define MATRIX_SIZE (ROWS * COLUMNS)
#define MAX_BULLETS_PER_PLAYER 5
#define MAX_BULLETS (MAX_BULLETS_PER_PLAYER * 2)

typedef struct{
  int x, y;
} player_pos;

typedef struct{
  int up, down, left, right, fire, quit;
}player_bind;

typedef struct{
  int direction;
  int x, y;
  bool available;
  int owner_id;
  bool active;
  pthread_t thread_id;
} bullet;

typedef struct{
  sem_t cell_semaphores[MATRIX_SIZE];
  sem_t status_sem;
  char tabla[MATRIX_SIZE];
  
  player_pos players[2];
  char plyr1, plyr2;
  int plyr1HP, plyr2HP;
  bool creator_quit;
  bool worker_quit;
  bool both_players_online;
  bool board_initialized;

  player_bind binds[2];
  bullet bullets[MAX_BULLETS];
} shared_matrix_t;