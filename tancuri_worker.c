#include "ncurses_fncs.c"

int main(int argc, char* argv[]){
  initscr();
  refresh();

  int fd;
  shared_matrix_t *shm_ptr;
  size_t shm_size = sizeof(shared_matrix_t);

  fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (fd == -1){
    printw("Eroare shm_open. Ruleaza tancuri_creator prima data!\n");
    exit(EXIT_FAILURE);
  }

  shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED){
    printw("Eroare mmap");
    exit(EXIT_FAILURE);
  }
  close(fd);

  print_board(shm_ptr->tabla);

  getch();
  if (munmap(shm_ptr, shm_size) == -1) {
    perror("munmap failed");
  }
  curs_set(1);
  endwin();
  return 0;
}