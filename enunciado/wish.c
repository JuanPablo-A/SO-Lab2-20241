// Author: Jose David Gómez Muñetón
// Author: Juan Pablo Arango Gaviria

// Este programa es un shell interactivo que permite ejecutar comandos integrados y externos. Los comandos integrados son:
// - exit: finaliza el shell.
// - cd <directory>: cambia el directorio de trabajo actual.
// - path <directory1> <directory2> ... <directoryN>: establece los directorios donde se buscarán los comandos externos.
//
// El shell también permite ejecutar comandos externos. Si el comando no se encuentra en el directorio actual, se buscará en los directorios establecidos con el comando path.

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INITIAL_CAPACITY 2
#define MAX_COMMAND_SIZE 1024

typedef struct
{
  char **entries;
  int size;
  int capacity;
} path_array_t;

typedef struct
{
  char **args;
  int size;
  int capacity;
} cmd_t;

typedef struct
{
  cmd_t **commands;
  int size;
  int capacity;
} cmd_list_t;

static path_array_t path_arr;
static cmd_list_t cmd_list;
static char *input_line;
static char *file_path;
static int exit_code;
static int batch_mode;

static void init_path_arr(path_array_t *arr)
{
  arr->size = 0;
  arr->capacity = INITIAL_CAPACITY;
  arr->entries = calloc(arr->capacity, sizeof(char *));
}

static void init_cmd(cmd_t *c)
{
  c->size = 0;
  c->capacity = INITIAL_CAPACITY;
  c->args = calloc(c->capacity, sizeof(char *));
}

static void init_cmd_list(cmd_list_t *cl)
{
  cl->size = 0;
  cl->capacity = INITIAL_CAPACITY;
  cl->commands = calloc(cl->capacity, sizeof(cmd_t *));
}

static void ensure_path_capacity(path_array_t *arr)
{
  if (arr->size == arr->capacity)
  {
    arr->capacity *= 2;
    arr->entries = realloc(arr->entries, arr->capacity * sizeof(char *));
  }
}

static void ensure_cmd_capacity(cmd_t *c)
{
  if (c->size == c->capacity)
  {
    c->capacity *= 2;
    c->args = realloc(c->args, c->capacity * sizeof(char *));
  }
}

static void ensure_cmd_list_capacity(cmd_list_t *cl)
{
  if (cl->size == cl->capacity)
  {
    cl->capacity *= 2;
    cl->commands = realloc(cl->commands, cl->capacity * sizeof(cmd_t *));
  }
}

static int check_executable_access(char *filename, char **out_fp)
{
  if (access(filename, X_OK) == 0)
  {
    strcpy(*out_fp, filename);
    return EXIT_SUCCESS;
  }
  for (int i = 0; i < path_arr.size; i++)
  {
    snprintf(*out_fp, MAX_COMMAND_SIZE, "%s/%s", path_arr.entries[i], filename);
    if (access(*out_fp, X_OK) == 0)
    {
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}

static int run_external_cmd(cmd_t *c)
{
  int ri = -1;
  for (int i = 0; i < c->size; i++)
  {
    if (strcmp(c->args[i], ">") == 0)
    {
      ri = i;
      if (ri == 0 || (c->size - (ri + 1)) != 1)
      {
        fprintf(stderr, "An error has occurred\n");
        return EXIT_FAILURE;
      }
      c->args[ri] = NULL;
      break;
    }
  }
  if (check_executable_access(c->args[0], &file_path) == EXIT_SUCCESS)
  {
    pid_t child = fork();
    if (child == -1)
    {
      fprintf(stderr, "error: %s\n", strerror(errno));
      exit_code = EXIT_FAILURE;
      return EXIT_SUCCESS;
    }
    if (child == 0)
    {
      if (ri != -1)
      {
        int fd = open(c->args[ri + 1], O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (fd == -1)
        {
          fprintf(stderr, "error: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      execv(file_path, c->args);
      fprintf(stderr, "error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    fprintf(stderr, batch_mode ? "An error has occurred\n" : "error:\n\tcommand '%s' not found\n", c->args[0]);
  }
  return EXIT_FAILURE;
}

static int run_cmd(cmd_t *c)
{
  if (c->size == 0 || c->args[0] == NULL)
  {
    return EXIT_FAILURE;
  }
  if (strcmp(c->args[0], "exit") == 0)
  {
    if (c->size > 1)
    {
      fprintf(stderr, "An error has occurred\n");
      return EXIT_FAILURE;
    }
    if (!batch_mode)
    {
      printf("Goodbye!\n");
    }
    return EXIT_SUCCESS;
  }
  if (strcmp(c->args[0], "cd") == 0)
  {
    if (c->size != 2)
    {
      fprintf(stderr, "An error has occurred\n");
      return EXIT_FAILURE;
    }
    if (chdir(c->args[1]) == -1)
    {
      fprintf(stderr, "error:\n\tcannot execute command 'cd': %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
  }
  if (strcmp(c->args[0], "path") == 0)
  {
    for (int i = 0; i < path_arr.size; i++)
    {
      free(path_arr.entries[i]);
    }
    path_arr.size = 0;
    for (int i = 1; i < c->size; i++)
    {
      ensure_path_capacity(&path_arr);
      path_arr.entries[path_arr.size] = malloc((strlen(c->args[i]) + 1) * sizeof(char));
      strcpy(path_arr.entries[path_arr.size++], c->args[i]);
    }
    return EXIT_FAILURE;
  }
  return run_external_cmd(c);
}

static void start_shell(FILE *in)
{
  while (1)
  {
    if (!batch_mode)
    {
      char *cwd = getcwd(NULL, 0);
      if (cwd)
      {
        printf("%s\n", cwd);
        free(cwd);
      }
      printf("wish>");
    }
    if (fgets(input_line, MAX_COMMAND_SIZE, in) == NULL || feof(in))
    {
      if (batch_mode)
      {
        break;
      }
      fprintf(stderr, "error: %s", strerror(errno));
      exit_code = EXIT_FAILURE;
      break;
    }
    cmd_list.size = 0;
    char *scp = NULL;
    char *sc_line = strtok_r(input_line, "&", &scp);
    while (sc_line != NULL)
    {
      ensure_cmd_list_capacity(&cmd_list);
      cmd_t *single_c = cmd_list.commands[cmd_list.size];
      if (single_c == NULL)
      {
        single_c = malloc(sizeof(cmd_t));
        init_cmd(single_c);
        cmd_list.commands[cmd_list.size] = single_c;
      }
      single_c->size = 0;
      char *tp = NULL;
      char *tk = strtok_r(sc_line, " \n\t", &tp);
      while (tk != NULL)
      {
        ensure_cmd_capacity(single_c);
        single_c->args[single_c->size++] = tk;
        tk = strtok_r(NULL, " \n\t", &tp);
      }
      single_c->args[single_c->size] = NULL;
      cmd_list.size++;
      sc_line = strtok_r(NULL, "&", &scp);
    }
    if (cmd_list.size > 0 && cmd_list.commands[cmd_list.size - 1]->size == 0)
    {
      cmd_list.size--;
    }
    int ex = EXIT_FAILURE;
    for (int i = 0; i < cmd_list.size; i++)
    {
      cmd_t *sc = cmd_list.commands[i];
      if (run_cmd(sc) == EXIT_SUCCESS)
      {
        ex = EXIT_SUCCESS;
        break;
      }
    }
    if (ex == EXIT_SUCCESS)
    {
      break;
    }
    for (int i = 0; i < cmd_list.size; i++)
    {
      wait(NULL);
    }
    if (!batch_mode)
    {
      printf("\n");
    }
  }
}

int main(int argc, char const *argv[])
{
  input_line = malloc(MAX_COMMAND_SIZE * sizeof(char));
  file_path = malloc(MAX_COMMAND_SIZE * sizeof(char));
  init_cmd_list(&cmd_list);
  init_path_arr(&path_arr);
  ensure_path_capacity(&path_arr);
  path_arr.entries[path_arr.size] = malloc(strlen("/bin") + 1);
  strcpy(path_arr.entries[path_arr.size++], "/bin");
  exit_code = 0;
  if (argc > 2)
  {
    fprintf(stderr, "An error has occurred\n");
    exit_code = EXIT_FAILURE;
    goto cl;
  }
  batch_mode = (argc == 2);
  FILE *in = batch_mode ? fopen(argv[1], "r") : stdin;
  if (batch_mode && in == NULL)
  {
    fprintf(stderr, "An error has occurred\n");
    exit_code = EXIT_FAILURE;
  }
  else
  {
    start_shell(in);
    if (batch_mode && in)
    {
      fclose(in);
    }
  }
cl:
  for (int i = 0; i < path_arr.size; i++)
  {
    free(path_arr.entries[i]);
  }
  free(path_arr.entries);
  for (int i = 0; i < cmd_list.capacity; i++)
  {
    if (cmd_list.commands[i] != NULL)
    {
      free(cmd_list.commands[i]->args);
      free(cmd_list.commands[i]);
    }
  }
  free(cmd_list.commands);
  free(input_line);
  free(file_path);
  exit(exit_code == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}