#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define ROWS 10
#define COLUMNS 10
// numele segmentului de memorie partajata
#define SHM_NAME "/shm_tabla"
#define MATRIX_SIZE (ROWS * COLUMNS)

typedef struct{
  sem_t cell_semaphores[MATRIX_SIZE];
  char tabla[MATRIX_SIZE];
} shared_matrix_t;