#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

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

struct{
  int last_direction;
} player_stats[2] = {{-1}, {-1}};

typedef struct {
  shared_matrix_t* shm_ptr;
  int bullet_index;
  char player1_char;
  char player2_char;
  bool both_players;
} bullet_thread_args;

int current_player = -1;
volatile sig_atomic_t terminate_flag = 0;
bool player_won = false;

void initializare_semafoare(shared_matrix_t *shm_ptr){
  if (sem_init(&(shm_ptr->status_sem), 1, 1) == -1){
    printw("Eroare sem_init pentru status_sem\n");
    exit(EXIT_FAILURE);
  }
  
  for (int i = 0; i < MATRIX_SIZE; i++){
    //int sem_init(sem_t *sem, int pshared, unsigned int value);
    if (sem_init(&(shm_ptr->cell_semaphores[i]), 1, 1) == -1){
      printw("Eroare sem_init\n");
      exit(EXIT_FAILURE);
    }
  }

  printw("Creator: %d semafoare initializate + 1 semafor de status.\n", MATRIX_SIZE);
}

void distrugere_semafoare(shared_matrix_t *shm_ptr){
  if (sem_destroy(&(shm_ptr->status_sem)) == -1){
    perror("Eroare sem_destroy pentru status_sem");
  }
  
  for (int i = 0; i < MATRIX_SIZE; i++){
    if (sem_destroy(&(shm_ptr->cell_semaphores[i])) == -1){
      perror("Eroare sem_destroy");
    }
  }
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
    if ((char)c == '\n' || (char)c == '\r') {
      continue;
    }
    tabla[i++] = (char)c;
  }
  fclose(file);
  return tabla;
}

void init_keys(shared_matrix_t* shm_ptr){
  printw("Configurare bind-uri taste:\n");
  printw("Introduceti comanda pentru SUS: ");
  shm_ptr->binds[current_player].up = getch();
  if(terminate_flag) return;
  printw("\n");

  printw("Introduceti comanda pentru JOS: ");
  shm_ptr->binds[current_player].down = getch();
  if(terminate_flag) return;
  printw("\n");

  printw("Introduceti comanda pentru STANGA: ");
  shm_ptr->binds[current_player].left = getch();
  if(terminate_flag) return;
  printw("\n");
  
  printw("Introduceti comanda pentru DREAPTA: ");
  shm_ptr->binds[current_player].right = getch();
  if(terminate_flag) return;
  printw("\n");

  printw("Introduceti comanda pentru FIRE: ");
  shm_ptr->binds[current_player].fire = getch();
  if(terminate_flag) return;
  printw("\n");

  printw("Introduceti comanda pentru QUIT: ");
  shm_ptr->binds[current_player].quit = getch();
  if(terminate_flag) return;
  printw("\n");
}

void move_player(int direction, shared_matrix_t* shm_ptr, int player_id){
  int old_x = shm_ptr->players[player_id].x;
  int old_y = shm_ptr->players[player_id].y;

  int new_x = old_x;
  int new_y = old_y;

  switch(direction){
    case 0: new_x -= 1; break;
    case 1: new_y -= 1; break;
    case 2: new_x += 1; break;
    case 3: new_y += 1; break;
    default: break;
  }

  int old_index = COLUMNS * old_x + old_y;
  int new_index = COLUMNS * new_x + new_y;

  int first_index = (old_index < new_index) ? old_index : new_index;
  int second_index = (old_index < new_index) ? new_index : old_index;

  if(sem_wait(&(shm_ptr->cell_semaphores[first_index])) == -1){
    printw("sem_wait failed first index\n");
    exit(EXIT_FAILURE);
  }
  if(sem_wait(&(shm_ptr->cell_semaphores[second_index])) == -1){
    sem_post(&(shm_ptr->cell_semaphores[first_index]));
    printw("sem_wait failed second index\n");
    exit(EXIT_FAILURE);
  }
  if(shm_ptr->tabla[new_index] == ' ' && new_x >= 0 && new_x < ROWS && new_y >= 0 && new_y < COLUMNS){
    shm_ptr->players[player_id].x = new_x;
    shm_ptr->players[player_id].y = new_y;
    player_stats[player_id].last_direction = direction;

    char plyr = (player_id == 0) ? shm_ptr->plyr1 : shm_ptr->plyr2;

    shm_ptr->tabla[old_index] = ' ';
    shm_ptr->tabla[new_index] = plyr;
  }

  if(sem_post(&(shm_ptr->cell_semaphores[second_index])) == -1){
    printw("sem_post failed");
    exit(EXIT_FAILURE);
  }
  if(sem_post(&(shm_ptr->cell_semaphores[first_index])) == -1){
    printw("sem_post failed");
    exit(EXIT_FAILURE);
  }
}

void* bullet_thread(void* arg) {
  bullet_thread_args* args = (bullet_thread_args*)arg;
  shared_matrix_t* shm_ptr = args->shm_ptr;
  int bullet_idx = args->bullet_index;
  
  bullet* curr_bullet = &shm_ptr->bullets[bullet_idx];
  
  while (!terminate_flag && !player_won) {
    if (curr_bullet->available) {
      break;
    }
    
    int old_x = curr_bullet->x;
    int old_y = curr_bullet->y;
    int new_x = old_x;
    int new_y = old_y;
    
    switch(curr_bullet->direction) {
      case 0: new_x -= 1; break; // UP
      case 1: new_y -= 1; break; // LEFT
      case 2: new_x += 1; break; // DOWN
      case 3: new_y += 1; break; // RIGHT
      default: break;
    }
    
    if (new_x < 0 || new_x >= ROWS || new_y < 0 || new_y >= COLUMNS) {
      int old_index = COLUMNS * old_x + old_y;
      
      sem_wait(&(shm_ptr->cell_semaphores[old_index]));
      shm_ptr->tabla[old_index] = ' ';
      curr_bullet->available = true;
      curr_bullet->active = false;
      sem_post(&(shm_ptr->cell_semaphores[old_index]));
      break;
    }
    
    int old_index = COLUMNS * old_x + old_y;
    int new_index = COLUMNS * new_x + new_y;
    
    int first_index = (old_index < new_index) ? old_index : new_index;
    int second_index = (old_index < new_index) ? new_index : old_index;
    
    sem_wait(&(shm_ptr->cell_semaphores[first_index]));
    if (first_index != second_index) {
      sem_wait(&(shm_ptr->cell_semaphores[second_index]));
    }
    
    char target = shm_ptr->tabla[new_index];
    
    if (target == '#') {
      shm_ptr->tabla[old_index] = ' ';
      curr_bullet->available = true;
      curr_bullet->active = false;
      
      if (first_index != second_index) {
        sem_post(&(shm_ptr->cell_semaphores[second_index]));
      }
      sem_post(&(shm_ptr->cell_semaphores[first_index]));
      break;
      
    } else if (target == args->player1_char) {
      shm_ptr->tabla[old_index] = ' ';
      curr_bullet->available = true;
      curr_bullet->active = false;
      
      if (shm_ptr->plyr1HP >= 10) {
        shm_ptr->plyr1HP -= 10;
      } else {
        shm_ptr->plyr1HP = 0;
        player_won = true;
      }
      
      if (first_index != second_index) {
        sem_post(&(shm_ptr->cell_semaphores[second_index]));
      }
      sem_post(&(shm_ptr->cell_semaphores[first_index]));
      break;
      
    } else if (target == args->player2_char && args->both_players) {
      shm_ptr->tabla[old_index] = ' ';
      curr_bullet->available = true;
      curr_bullet->active = false;
      
      if (shm_ptr->plyr2HP >= 10) {
        shm_ptr->plyr2HP -= 10;
      } else {
        shm_ptr->plyr2HP = 0;
        player_won = true;
      }
      
      if (first_index != second_index) {
        sem_post(&(shm_ptr->cell_semaphores[second_index]));
      }
      sem_post(&(shm_ptr->cell_semaphores[first_index]));
      break;
      
    } else if (target == '.') {
      shm_ptr->tabla[old_index] = ' ';
      shm_ptr->tabla[new_index] = ' ';
      
      curr_bullet->available = true;
      curr_bullet->active = false;
      
      for (int i = 0; i < MAX_BULLETS; i++) {
        if (i != bullet_idx && !shm_ptr->bullets[i].available &&
            shm_ptr->bullets[i].x == new_x && shm_ptr->bullets[i].y == new_y) {
          shm_ptr->bullets[i].available = true;
          shm_ptr->bullets[i].active = false;
          break;
        }
      }
      
      if (first_index != second_index) {
        sem_post(&(shm_ptr->cell_semaphores[second_index]));
      }
      sem_post(&(shm_ptr->cell_semaphores[first_index]));
      break;
      
    } else {
      curr_bullet->x = new_x;
      curr_bullet->y = new_y;
      
      shm_ptr->tabla[old_index] = ' ';
      shm_ptr->tabla[new_index] = '.';
      
      if (first_index != second_index) {
        sem_post(&(shm_ptr->cell_semaphores[second_index]));
      }
      sem_post(&(shm_ptr->cell_semaphores[first_index]));
    }

    usleep(50000);
  }
  
  free(args);
  return NULL;
}

void player_fire(shared_matrix_t* shm_ptr, int player_id){
  int start_idx = player_id * MAX_BULLETS_PER_PLAYER;
  int end_idx = start_idx + MAX_BULLETS_PER_PLAYER;
  
  for (int i = start_idx; i < end_idx; i++) {
    bullet* curr_bullet = &shm_ptr->bullets[i];
    
    if (player_stats[player_id].last_direction != -1 && curr_bullet->available) {
      int player_x = shm_ptr->players[player_id].x;
      int player_y = shm_ptr->players[player_id].y;
      int new_x = player_x;
      int new_y = player_y;
      
      switch(player_stats[player_id].last_direction){
        case 0: // UP
          if (player_x - 1 >= 0){
            curr_bullet->direction = 0;
            new_x = player_x - 1; 
            new_y = player_y;
          } else {
            continue;
          }
          break;
        case 1: // LEFT
          if (player_y - 1 >= 0){
            curr_bullet->direction = 1;
            new_x = player_x; 
            new_y = player_y - 1;
          } else {
            continue;
          }
          break;
        case 2: // DOWN
          if (player_x + 1 < ROWS){
            curr_bullet->direction = 2;
            new_x = player_x + 1; 
            new_y = player_y;
          } else {
            continue;
          }
          break;
        case 3: // RIGHT
          if (player_y + 1 < COLUMNS){
            curr_bullet->direction = 3;
            new_x = player_x; 
            new_y = player_y + 1;
          } else {
            continue;
          }
          break;
        default: 
          continue;
      }
      
      curr_bullet->x = new_x;
      curr_bullet->y = new_y;
      curr_bullet->owner_id = player_id;
      curr_bullet->available = false;
      curr_bullet->active = true;

      bullet_thread_args* args = malloc(sizeof(bullet_thread_args));
      if (args == NULL) {
        curr_bullet->available = true;
        curr_bullet->active = false;
        return;
      }
      
      args->shm_ptr = shm_ptr;
      args->bullet_index = i;
      args->player1_char = shm_ptr->plyr1;
      args->player2_char = shm_ptr->plyr2;
      args->both_players = shm_ptr->both_players_online;
      
      if (pthread_create(&curr_bullet->thread_id, NULL, bullet_thread, args) != 0) {
        free(args);
        curr_bullet->available = true;
        curr_bullet->active = false;
        return;
      }
      
      pthread_detach(curr_bullet->thread_id);
      
      return;
    }
  }
}

int handle_input(int key, shared_matrix_t* shm_ptr){
  if (key == shm_ptr->binds[0].quit){
    sem_wait(&shm_ptr->status_sem);
    shm_ptr->creator_quit = true;
    sem_post(&shm_ptr->status_sem);
  }
  if (key == shm_ptr->binds[0].up){
    move_player(0, shm_ptr, 0);
  }
  if (key == shm_ptr->binds[0].left){
    move_player(1, shm_ptr, 0);
  }
  if (key == shm_ptr->binds[0].down){
    move_player(2, shm_ptr, 0);
  }
  if (key == shm_ptr->binds[0].right){
    move_player(3, shm_ptr, 0);
  }
  if (key == shm_ptr->binds[0].fire){
    player_fire(shm_ptr, 0);
  }

  if (shm_ptr->both_players_online) {
    if (key == shm_ptr->binds[1].quit){
      sem_wait(&shm_ptr->status_sem);
      shm_ptr->worker_quit = true;
      sem_post(&shm_ptr->status_sem);
    }
    if (key == shm_ptr->binds[1].up){
      move_player(0, shm_ptr, 1);
    }
    if (key == shm_ptr->binds[1].left){
      move_player(1, shm_ptr, 1);
    }
    if (key == shm_ptr->binds[1].down){
      move_player(2, shm_ptr, 1);
    }
    if (key == shm_ptr->binds[1].right){
      move_player(3, shm_ptr, 1);
    }
    if (key == shm_ptr->binds[1].fire){
      player_fire(shm_ptr, 1);
    }
  }
  
  return 1;
}

void cleanup_and_exit(shared_matrix_t* shm_ptr, size_t shm_size, int is_creator, int exit_code, bool ncurses_initialized){
  if (ncurses_initialized) {
    curs_set(1);
    endwin();
  }
  
  if (shm_ptr != NULL && shm_ptr != MAP_FAILED) {
    if (munmap(shm_ptr, shm_size) == -1) {
      perror("munmap failed during cleanup");
    }
  }
  
  if (is_creator) {
    if (shm_unlink(SHM_NAME) == -1) {
      perror("shm_unlink failed during cleanup");
    }
  }
  
  exit(exit_code);
}

void signal_handler(int signum){
  if (signum == SIGINT) {
    terminate_flag = 1;
  }
}

int main(int argc, char* argv[]){
  if (argc != 3){
    fprintf(stderr, "Utilizare: %s <cale_fisier> identificator_jucator (0 - creator, 1 - worker)\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int plyrID = atoi(argv[2]);
  if (plyrID != 0 && plyrID != 1){
    fprintf(stderr, "Jucatorul curent trebuie sa fie 0 - creator sau 1 - worker!\n");
    exit(EXIT_FAILURE);
  }
  current_player = plyrID;

  int fd;
  shared_matrix_t *shm_ptr;
  size_t shm_size = sizeof(shared_matrix_t);

  sem_t *init_sem = NULL; // semafor pentru verificare memorie partajata initializata

  if (current_player == 0){
    if(signal(SIGINT, signal_handler) == SIG_ERR){
      fprintf(stderr, "Eroare la setarea signal handler-ului.\n");
      exit(EXIT_FAILURE);
    }

    // creez zona mem partajata -> 0666 permisiuni (Ignorat, owner, grup, altii) -> 6 (citire 4, scriere 2, executare 0)
    fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd == -1){
      fprintf(stderr, "Eroare shm_open creator - Un joc este deja in curs! Un alt proces creator ruleaza deja.\n");
      exit(EXIT_FAILURE);
    }
    // setez dimensiunea zonei de memorie partajata
    if (ftruncate(fd, shm_size) == -1){
      fprintf(stderr, "Eroare ftruncate\n");
      exit(EXIT_FAILURE);
    }
    printf("Creator -> segmentul de memorie a fost creat\n");
  }else if (current_player == 1){
    if(signal(SIGINT, signal_handler) == SIG_ERR){
      fprintf(stderr, "Eroare la setarea signal handler-ului.\n");
      exit(EXIT_FAILURE);
    }
    
    fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1){
      fprintf(stderr, "Eroare shm_open worker | Rulati prima data procesul creator (jucatorul 0)\n");
      exit(EXIT_FAILURE);
    }
  }

  //void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
  shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_ptr == MAP_FAILED){
    fprintf(stderr, "Eroare mmap()\n");
    close(fd);
    if (current_player == 0) {
      shm_unlink(SHM_NAME);
    }
    exit(EXIT_FAILURE);
  }
  close(fd);
  
  int offset_x = 2; // offset pentru pozitionare jucatori pe tabla
  int offset_y = 2; 
  if (current_player == 0){
    init_sem = sem_open(INIT_SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (init_sem == SEM_FAILED) {
      fprintf(stderr, "Eroare sem_open creator. Un alt joc ruleaza.\n");
      perror("sem_open");
      shm_unlink(SHM_NAME);
      cleanup_and_exit(shm_ptr, shm_size, current_player == 0, EXIT_SUCCESS, true);
    }

    initializare_semafoare(shm_ptr);
    
    shm_ptr->board_initialized = false;
    
    // citire tabla din fisier.txt -> incarcare in zona de memorie partajata
    char* tabla_citita = citire_fisier(argv[1]);
    for(int i = 0; i < MATRIX_SIZE; i++){
      shm_ptr->tabla[i] = tabla_citita[i];
    }
    free(tabla_citita);
    player_pos* current_player = &shm_ptr->players[0];
    
    current_player->x = 0 + offset_x;
    current_player->y = 0 + offset_y;

    shm_ptr->plyr1HP = 100;
    shm_ptr->plyr2HP = 100;

    for(int i = 0; i < MAX_BULLETS; i++){
      shm_ptr->bullets[i].available = true;
      shm_ptr->bullets[i].active = false;
      shm_ptr->bullets[i].owner_id = (i < MAX_BULLETS_PER_PLAYER) ? 0 : 1;
      shm_ptr->bullets[i].x = -1;
      shm_ptr->bullets[i].y = -1;
      shm_ptr->bullets[i].direction = -1;
    }

    shm_ptr->both_players_online = false;

    sem_wait(&shm_ptr->status_sem);
    shm_ptr->creator_quit = false;
    shm_ptr->worker_quit = false;
    shm_ptr->board_initialized = true;
    sem_post(&shm_ptr->status_sem);

    if (sem_post(init_sem) == -1) {
        perror("sem_post init_sem creator");
    }

    sem_close(init_sem);
  }else if(current_player == 1){
    sem_t *init_sem_worker;

    init_sem_worker = sem_open(INIT_SEM_NAME, 0);
    if (init_sem_worker == SEM_FAILED) {
        fprintf(stderr, "Eroare sem_open worker.\n");
        munmap(shm_ptr, shm_size);
        cleanup_and_exit(shm_ptr, shm_size, current_player == 1, EXIT_FAILURE, true);
    }

    if (sem_wait(init_sem_worker) == -1) {
        sem_close(init_sem_worker);
        munmap(shm_ptr, shm_size);
        cleanup_and_exit(shm_ptr, shm_size, current_player == 1, EXIT_FAILURE, true);
    }

    sem_close(init_sem_worker);
    
    if(terminate_flag){
      fprintf(stderr, "Worker-ul a fost intrerupt inainte de initializarea tablei.\n");
      munmap(shm_ptr, shm_size);
      cleanup_and_exit(shm_ptr, shm_size, current_player == 1, EXIT_FAILURE, true);
    }
    
    player_pos* current_player = &shm_ptr->players[1];
    current_player->x = ROWS - 1 - offset_x;
    current_player->y = COLUMNS - 1 - offset_y;

    shm_ptr->tabla[current_player->x * COLUMNS + current_player->y] = shm_ptr->plyr2;
    shm_ptr->both_players_online = true;
  }
  
  initscr();
  cbreak();
  keypad(stdscr, TRUE);
  init_keys(shm_ptr);
  refresh();

  char plyr;
  if(!terminate_flag){
    printw("Introduceti caracterul jucatorului: ");
    plyr = getch();
  }
  while(!isalpha(plyr) && terminate_flag == 0){
    clear();
    refresh();
    printw("Caracterul introdus trebuie sa fie o litera %c!\n", plyr);
    printw("Introduceti caracterul jucatorului: ");
    plyr = getch();
  }
  
  if(terminate_flag){
    cleanup_and_exit(shm_ptr, shm_size, current_player == 0, EXIT_SUCCESS, true);
  }
  
  if(current_player == 0){
    shm_ptr->plyr1 = plyr;
    shm_ptr->plyr2 = ' ';

    shm_ptr->tabla[shm_ptr->players[0].x * COLUMNS + shm_ptr->players[0].y] = shm_ptr->plyr1;
  }else if(current_player == 1){
    shm_ptr->plyr2 = plyr;

    shm_ptr->tabla[shm_ptr->players[1].x * COLUMNS + shm_ptr->players[1].y] = shm_ptr->plyr2;
  }

  noecho();
  nodelay(stdscr, TRUE);
  clear();
  refresh();

  int key;
  while(true){
    if (terminate_flag == 1) {
      sem_wait(&shm_ptr->status_sem);
      if (current_player == 0) {
        shm_ptr->creator_quit = true;
      } else if (current_player == 1) {
        shm_ptr->worker_quit = true;
      }
      sem_post(&shm_ptr->status_sem);
      break;
    }
    
    sem_wait(&shm_ptr->status_sem);
    bool should_quit = (shm_ptr->creator_quit == 1 || shm_ptr->worker_quit == true);
    sem_post(&shm_ptr->status_sem);
    
    if (should_quit){
      break;
    }

    if(shm_ptr->plyr1HP <= 0 || shm_ptr->plyr2HP <= 0){
      player_won = true;
    }
    
    if(player_won){
      clear();
      refresh();
      break;
    }

    print_board(shm_ptr->tabla,                     // tabla
      shm_ptr->players[0].x, shm_ptr->players[0].y, // coordonate player 1 
      shm_ptr->players[1].x, shm_ptr->players[1].y, // coordonate player 2
      shm_ptr->plyr1, shm_ptr->plyr2,               // caractere player1, player2
      shm_ptr->bullets,                             // gloantele de pe tabla
      shm_ptr->plyr1HP, shm_ptr->plyr2HP            // HP jucători
    );
    
    key = getch();
    if(key != ERR){
      handle_input(key, shm_ptr); 
    }
    
    napms(20);
  }

  int final_plyr1HP = shm_ptr->plyr1HP;
  int final_plyr2HP = shm_ptr->plyr2HP;
  char final_plyr1 = shm_ptr->plyr1;
  char final_plyr2 = shm_ptr->plyr2;

  curs_set(1);
  endwin();
  
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (shm_ptr->bullets[i].active) {
      shm_ptr->bullets[i].available = true;
    }
  }

  usleep(100000);
  
  sem_wait(&shm_ptr->status_sem);
  bool creator_has_quit = shm_ptr->creator_quit;
  bool worker_has_quit = shm_ptr->worker_quit;
  sem_post(&shm_ptr->status_sem);
  
  if(creator_has_quit){
    fprintf(stderr, "Creatorul a parasit jocul.\n");
  }
  
  if(worker_has_quit){
    fprintf(stderr, "Workerul a parasit jocul.\n");
  }

  if(current_player == 0){
    distrugere_semafoare(shm_ptr);
  }

  if (munmap(shm_ptr, shm_size) == -1) {
      perror("munmap failed");
  }

  if(player_won){
    if(final_plyr1HP <= 0){
      printf("Player 2 (%c) WON!\n", final_plyr2);
    }else if(final_plyr2HP <= 0){
      printf("Player 1 (%c) WON!\n", final_plyr1);
    }
  }else{
    printf("Game ended.\n");
  }

  if(current_player == 0){
    if(shm_unlink(SHM_NAME) == -1){
      perror("Eroare shm_unlink creator");
    } else {
      printf("Creator -> segmentul de memorie a fost sters\n");
    }
  }
  return 0;
}
