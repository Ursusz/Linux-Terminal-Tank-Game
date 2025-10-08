#include <stdio.h>
#include <stdlib.h>

#define ROWS 10
#define COLUMNS 10
#define MATRIX_SIZE (ROWS * COLUMNS)

char* citire_fisier(char* file_path){
  FILE *file = fopen(file_path, "r");
  if (file == NULL){
    perror("Eroare citire fisier tabla.\n");
    exit(EXIT_FAILURE);
  }

  char* tabla = (char*)malloc(MATRIX_SIZE * sizeof(char));
  if (tabla == NULL){
    perror("Eroare alocare memorie tabla");
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
  if (argc != 2){
    fprintf(stderr, "Utilizare: %s <cale_fisier_tabla>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char* tabla = citire_fisier(argv[1]);

  for (int i = 0; i < MATRIX_SIZE;){
    printf("%c", tabla[i] == ' ' ? '_' : tabla[i]);         
    if(++i % COLUMNS == 0){
      printf("\n");
    }
  }
  free(tabla);

  return 0;
}

