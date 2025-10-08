#include <stdio.h>
#include <stdlib.h>

#include "ncurses_fncs.c"

/*
1. Implementati un joc de tancuri care se poate juca in doi, la o aceeasi
 tastatura. Tancurile sunt caractere-litera (care identifica jucatorul),
 trag cu caractere-punct si se deplaseaza pe o tabla care contine si ziduri
 (labirint); tancul se poate deplasa de la tastatura sus, jos, dreapta,
 stanga; proiectilul e tras la comanda, in directia de deplasare a
 tancului; proiectilul se deplaseaza animat pe tabla pana loveste un tanc,
 un zid sau marginea tablei; daca loveste un tanc, i se scade din viata
 (vietile sunt afisate in colturi); jucatorul care ajunge cu viata la 0
 pierde.
   Tabla va fi implementata ca o matrice de pozitii alocata intr-un segnment
 de memorie partajata; fiecare jucator va opera pe tabla respectiva cu un
 proces propriu - jocul tanc este lansat de un jucator lansand un acelasi
 program, caruia i se dau ca argumente in linia de comanda tabla, litera
 jucatorului, comenzile de deplasare si foc; pozitiile pe tabla sunt protejate
 de accesarea simultana prin semafoare. Interfata va fi ncurses.
*/

void moving_and_sleeping()
{
    int row = 5;
    int col = 0;

    curs_set(0);

    for(char c = 65; c <= 90; c++)
    {
        move(row++, col++);
        addch(c);
        refresh();
        napms(100);
    }

    row = 5;
    col = 3;

    for(char c = 97; c <= 122; c++)
    {
        mvaddch(row++, col++, c);
        refresh();
        napms(100);
    }

    curs_set(1);

    addch('\n');
}

void initializare_semafoare(shared_matrix_t *shm_ptr){
  for (int i = 0; i < MATRIX_SIZE; i++){
    //int sem_init(sem_t *sem, int pshared, unsigned int value);
    if (sem_init(&(shm_ptr->cell_semaphores[i]), 1, 1) == -1){
      printw("Eroare sem_init\n");
      exit(EXIT_FAILURE);
    }
  }

  printw("Creator: %d semafoare initializate.\n", MATRIX_SIZE);
}

char* citire_fisier(char* file_path){
  FILE *file = fopen(file_path, "r");
  if (file == NULL){
    printw("Eroare citire fisier tabla.\n");
    exit(EXIT_FAILURE);
  }

  char* tabla = (char*)malloc(MATRIX_SIZE * sizeof(char));
  if (tabla == NULL){
    printw("Eroare alocare memorie tabla");
    fclose(file);
    exit(EXIT_FAILURE);
  }

  int i = 0;
  int c;
  while((i < MATRIX_SIZE) && ((c = fgetc(file)) != EOF)){
    tabla[i++] = (char)c;
    if (i % COLUMNS == 0){
      int newline = fgetc(file);
    }
  }
  fclose(file);
  return tabla;
}

int main(int argc, char* argv[]){
  initscr();
  refresh();

  if (argc != 2){
    printw("Utilizare: %s <cale_fisier>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int fd;
  shared_matrix_t *shm_ptr;
  size_t shm_size = sizeof(shared_matrix_t);

  // creez zoma mem partajata -> 0666 permisiuni (Ignorat, owner, grup, altii) -> 6 (citire 4, scriere 2, executare 0)
  fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd == -1){
    printw("Eroare shm_open\n");
    exit(EXIT_FAILURE);
  }

  // setez dimensiunea zonei de memorie partajata
  if (ftruncate(fd, shm_size) == -1){
    printw("Eroare ftruncate\n");
    exit(EXIT_FAILURE);
  }
  printw("Creator -> segmentul de memorie a fost creat\n");

  //void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
  shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED){
    printw("Eroare mmap()\n");
    exit(EXIT_FAILURE);
  }
  close(fd);

  initializare_semafoare(shm_ptr);

  // citire tabla din fisier.txt -> incarcare in zona de memorie partajata
  char* tabla_citita = citire_fisier(argv[1]);
  for(int i = 0; i < MATRIX_SIZE; i++){
    shm_ptr->tabla[i] = tabla_citita[i];
  }
  free(tabla_citita);

  /*
   if (sem_wait(&(shm_ptr->cell_semaphores[target_index])) == -1) {
        perror("sem_wait failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Worker: Semafor obținut. Citește valoarea veche: %d\n", 
           shm_ptr->data[target_index]);
           
    // Simulare de procesare
    shm_ptr->data[target_index] *= 2; 

    printf("Worker: Valoare nouă scrisă: %d\n", shm_ptr->data[target_index]);
    
    // Operația V (signal/semnalizează): Eliberează accesul
    if (sem_post(&(shm_ptr->cell_semaphores[target_index])) == -1) {
        perror("sem_post failed");
    }
    printf("Worker: Semafor eliberat.\n");
    // --- SFÂRȘIT SECȚIUNE CRITICĂ ---
  */

  print_board(shm_ptr->tabla);
  // moving_and_sleeping();
  getch();
  if (munmap(shm_ptr, shm_size) == -1) {
    perror("munmap failed");
  }
  curs_set(1);
  endwin();
  return 0;
}