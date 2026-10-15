#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

static pid_t *child_pids = NULL;
static int child_count = 0;
static volatile sig_atomic_t timed_out = 0;

// Обработчик SIGALRM: по истечении таймаута убиваем всех ещё живых детей
static void alarm_handler(int sig) {
  (void)sig;
  timed_out = 1;
  for (int i = 0; i < child_count; i++) {
    if (child_pids[i] > 0) kill(child_pids[i], SIGKILL);
  }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = 0;  // 0 - таймаут не задан
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"timeout", required_argument, 0, 0},
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
            timeout = atoi(optarg);
            if (timeout <= 0) {
              printf("timeout must be a positive number\n");
              return 1;
            }
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
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" "
           "[--timeout \"sec\"] [--by_files]\n",
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

  child_pids = calloc(pnum, sizeof(pid_t));
  if (child_pids == NULL) {
    printf("calloc failed\n");
    free(array);
    return 1;
  }

  int pipefd[2] = {-1, -1};
  if (!with_files) {
    if (pipe(pipefd) == -1) {
      perror("pipe");
      free(array);
      free(child_pids);
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
          sleep(5);
          if (write(pipefd[1], &local, sizeof(local)) != sizeof(local)) {
            perror("write");
            _exit(1);
          }
          close(pipefd[1]);
        }
        return 0;
      }
      // parent: запоминаем pid ребёнка
      child_pids[i] = child_pid;
      child_count = i + 1;

    } else {
      printf("Fork failed!\n");
      free(array);
      free(child_pids);
      return 1;
    }
  }

  if (!with_files) close(pipefd[1]);

  // Если задан таймаут - ставим будильник
  if (timeout > 0) {
    signal(SIGALRM, alarm_handler);
    alarm(timeout);
  }

  // Неблокирующее ожидание детей
  while (active_child_processes > 0) {
    int status;
    pid_t r = waitpid(-1, &status, WNOHANG);
    if (r > 0) {
      for (int i = 0; i < child_count; i++) {
        if (child_pids[i] == r) child_pids[i] = 0;  // уже завершён
      }
      active_child_processes -= 1;
    } else if (r == 0) {
      usleep(1000);  // дети ещё работают
    } else if (errno == EINTR) {
      continue;
    } else {
      perror("waitpid");
      break;
    }
  }

  if (timeout > 0) alarm(0);  // все дети завершились - будильник не нужен

  if (timed_out) {
    printf("Timeout (%d s) expired: remaining children were killed\n", timeout);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  int received = 0;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;
    struct MinMax local;

    if (with_files) {
      char filename[64];
      snprintf(filename, sizeof(filename), "minmax_%d.bin", i);
      FILE *f = fopen(filename, "rb");
      if (f == NULL || fread(&local, sizeof(local), 1, f) != 1) {
        if (f) fclose(f);
        remove(filename);
        if (timed_out) continue;  // ребёнок убит, результата нет
        printf("Failed to read %s\n", filename);
        free(array);
        free(child_pids);
        return 1;
      }
      fclose(f);
      remove(filename);
      min = local.min;
      max = local.max;
    } else {
      ssize_t n = read(pipefd[0], &local, sizeof(local));
      if (n == 0 && timed_out) break;  // EOF: остальные дети убиты
      if (n != sizeof(local)) {
        printf("Failed to read from pipe\n");
        free(array);
        free(child_pids);
        return 1;
      }
      min = local.min;
      max = local.max;
    }

    received++;
    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  if (!with_files) close(pipefd[0]);

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  free(child_pids);

  if (received == 0) {
    printf("No results received\n");
  } else {
    if (received < pnum) {
      printf("Result is partial: %d of %d processes finished\n", received, pnum);
    }
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
  }
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}