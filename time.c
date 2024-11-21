// Author: Jose David Gómez Muñetón
// Author: Juan Pablo Arango Gaviria

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Ejecuta un comando y mide el tiempo que tarda en ejecutarse.
 *
 * use: ./time <command> [command args...]
 *
 * args:
 *  - command: comando a ejecutar.
 *  - command args: argumentos del comando.
 *
 * return:
 *  - 0 si el comando se ejecutó correctamente.
 *  - 1 si ocurrió un error al ejecutar el comando.
 */
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Uso: %s <command> [command args...]\n", argv[0]);
    return EXIT_FAILURE;
  }

  struct timeval start_time, end_time;
  pid_t pid;

  if (gettimeofday(&start_time, NULL) != 0)
  {
    perror("Error al obtener el tiempo inicial");
    return EXIT_FAILURE;
  }

  pid = fork();

  if (pid < 0)
  {
    perror("Error al hacer fork");
    return EXIT_FAILURE;
  }
  else if (pid == 0)
  {
    execvp(argv[1], &argv[1]);
    perror("Error al ejecutar el comando");
    exit(EXIT_FAILURE);
  }
  else
  {
    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
      perror("Error al esperar al proceso hijo");
      return EXIT_FAILURE;
    }

    if (gettimeofday(&end_time, NULL) != 0)
    {
      perror("Error al obtener el tiempo final");
      return EXIT_FAILURE;
    }

    long seconds = end_time.tv_sec - start_time.tv_sec;
    long microseconds = end_time.tv_usec - start_time.tv_usec;
    double elapsed = seconds + microseconds * 1e-6;

    printf("Tiempo transcurrido: %.6f segundos\n", elapsed);

    if (WIFEXITED(status))
    {
      return WEXITSTATUS(status);
    }
    else
    {
      return EXIT_FAILURE;
    }
  }
}
