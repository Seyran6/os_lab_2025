#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;

static int fixed = 0; /* 1 - режим без дедлока */

/* Поток 1: берёт A, затем B */
static void *thread1(void *arg) {
  (void)arg;

  printf("[thread 1] locking A...\n");
  pthread_mutex_lock(&mutex_a);
  printf("[thread 1] locked A\n");

  sleep(1); /* даём второму потоку время захватить свой мьютекс */

  printf("[thread 1] locking B...\n");
  pthread_mutex_lock(&mutex_b);
  printf("[thread 1] locked B\n");

  printf("[thread 1] working with A and B\n");

  pthread_mutex_unlock(&mutex_b);
  pthread_mutex_unlock(&mutex_a);
  printf("[thread 1] done\n");
  return NULL;
}

/* Поток 2: в режиме deadlock берёт B, затем A (обратный порядок!).
 * В режиме fix берёт A, затем B (тот же порядок, что и поток 1). */
static void *thread2(void *arg) {
  (void)arg;

  pthread_mutex_t *first = fixed ? &mutex_a : &mutex_b;
  pthread_mutex_t *second = fixed ? &mutex_b : &mutex_a;
  const char *first_name = fixed ? "A" : "B";
  const char *second_name = fixed ? "B" : "A";

  printf("[thread 2] locking %s...\n", first_name);
  pthread_mutex_lock(first);
  printf("[thread 2] locked %s\n", first_name);

  sleep(1);

  printf("[thread 2] locking %s...\n", second_name);
  pthread_mutex_lock(second);
  printf("[thread 2] locked %s\n", second_name);

  printf("[thread 2] working with A and B\n");

  pthread_mutex_unlock(second);
  pthread_mutex_unlock(first);
  printf("[thread 2] done\n");
  return NULL;
}

int main(int argc, char **argv) {
  /* Построчный вывод, чтобы он не терялся при зависании программы */
  setvbuf(stdout, NULL, _IOLBF, 0);

  if (argc > 1 && strcmp(argv[1], "fix") == 0)
    fixed = 1;

  printf("Mode: %s\n", fixed ? "fixed (same lock order)" : "deadlock");

  pthread_t t1, t2;

  if (pthread_create(&t1, NULL, thread1, NULL) != 0) {
    perror("pthread_create");
    exit(1);
  }
  if (pthread_create(&t2, NULL, thread2, NULL) != 0) {
    perror("pthread_create");
    exit(1);
  }

  /* В режиме deadlock здесь программа зависнет навсегда */
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("All done, no deadlock\n");
  return 0;
}