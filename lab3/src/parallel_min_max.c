#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed < 0) {
              printf("seed must be a non-negative number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size must be a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum must be a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n",
           argv[0]);
    return 1;
  }

  // процессов не может быть больше, чем элементов
  if (pnum > array_size) pnum = array_size;

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    printf("malloc failed\n");
    return 1;
  }
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  int pipefd[2] = {-1, -1};
  if (!with_files) {
    if (pipe(pipefd) == -1) {
      perror("pipe");
      free(array);
      return 1;
    }
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process:
        unsigned int begin = (unsigned int)((long long)array_size * i / pnum);
        unsigned int end = (unsigned int)((long long)array_size * (i + 1) / pnum);

        struct MinMax local = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[64];
          snprintf(filename, sizeof(filename), "minmax_%d.bin", i);
          FILE *f = fopen(filename, "wb");
          if (f == NULL) {
            perror("fopen");
            _exit(1);
          }
          fwrite(&local, sizeof(local), 1, f);
          fclose(f);
        } else {
          close(pipefd[0]);
          if (write(pipefd[1], &local, sizeof(local)) != sizeof(local)) {
            perror("write");
            _exit(1);
          }
          close(pipefd[1]);
        }
        return 0;
      }

    } else {
      printf("Fork failed!\n");
      free(array);
      return 1;
    }
  }

  if (!with_files) close(pipefd[1]);

  while (active_child_processes > 0) {
    wait(NULL);
    active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;
    struct MinMax local;

    if (with_files) {
      char filename[64];
      snprintf(filename, sizeof(filename), "minmax_%d.bin", i);
      FILE *f = fopen(filename, "rb");
      if (f == NULL || fread(&local, sizeof(local), 1, f) != 1) {
        printf("Failed to read %s\n", filename);
        if (f) fclose(f);
        free(array);
        return 1;
      }
      fclose(f);
      remove(filename);
      min = local.min;
      max = local.max;
    } else {
      if (read(pipefd[0], &local, sizeof(local)) != sizeof(local)) {
        printf("Failed to read from pipe\n");
        free(array);
        return 1;
      }
      min = local.min;
      max = local.max;
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  if (!with_files) close(pipefd[0]);

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}