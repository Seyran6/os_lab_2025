#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sum.h"
#include "utils.h"

static void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  sum_args->result = Sum(sum_args);
  return NULL;
}

static void Usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n",
          prog);
}

static int ParseNumber(const char *s) {
  int v = atoi(s);
  return v < 0 ? -1 : v;
}

int main(int argc, char **argv) {
  int threads_num = -1;
  int array_size = -1;
  int seed = -1;

  static struct option options[] = {{"threads_num", required_argument, 0, 't'},
                                    {"seed", required_argument, 0, 's'},
                                    {"array_size", required_argument, 0, 'a'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
      case 't': threads_num = ParseNumber(optarg); break;
      case 's': seed = ParseNumber(optarg); break;
      case 'a': array_size = ParseNumber(optarg); break;
      default: Usage(argv[0]); return 1;
    }
  }

  if (threads_num <= 0 || array_size <= 0 || seed < 0 || optind != argc) {
    Usage(argv[0]);
    return 1;
  }
  if (threads_num > array_size) threads_num = array_size;  

  int *array = malloc(sizeof(int) * (size_t)array_size);
  pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)threads_num);
  struct SumArgs *args = malloc(sizeof(struct SumArgs) * (size_t)threads_num);
  if (!array || !threads || !args) {
    fprintf(stderr, "Error: out of memory\n");
    return 1;
  }

  GenerateArray(array, (unsigned int)array_size, (unsigned int)seed);

  int chunk = array_size / threads_num;
  for (int i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * chunk;

    args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * chunk;
    args[i].result = 0;
  }

  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; i < threads_num; i++) {
    if (pthread_create(&threads[i], NULL, ThreadSum, &args[i])) {
      fprintf(stderr, "Error: pthread_create failed!\n");
      return 1;
    }
  }

  long long total_sum = 0;
  for (int i = 0; i < threads_num; i++) {
    pthread_join(threads[i], NULL);
    total_sum += args[i].result;
  }

  clock_gettime(CLOCK_MONOTONIC, &finish);
  double elapsed_ms = (finish.tv_sec - start.tv_sec) * 1000.0 +
                      (finish.tv_nsec - start.tv_nsec) / 1e6;

  free(array);
  free(threads);
  free(args);

  printf("Total: %lld\n", total_sum);
  printf("Time: %.3f ms\n", elapsed_ms);
  return 0;
}