#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* Данные для одного потока: диапазон [begin, end] множителей */
struct thread_args {
  unsigned long long begin;
  unsigned long long end;
  unsigned long long mod;
};

/* Общий результат и мьютекс, защищающий его */
static unsigned long long result = 1;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

static void *partial_factorial(void *arg) {
  struct thread_args *a = (struct thread_args *)arg;
  unsigned long long local = 1 % a->mod;

  /* Считаем свою часть без блокировок: данные только локальные */
  for (unsigned long long i = a->begin; i <= a->end; i++)
    local = (local * (i % a->mod)) % a->mod;

  /* Критическая секция: только обновление общего результата */
  pthread_mutex_lock(&mut);
  result = (result * local) % a->mod;
  pthread_mutex_unlock(&mut);

  return NULL;
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s -k <number> --pnum=<threads> --mod=<modulus>\n",
          prog);
}

int main(int argc, char **argv) {
  long long k = -1, pnum = -1, mod = -1;

  static struct option options[] = {{"k", required_argument, 0, 'k'},
                                    {"pnum", required_argument, 0, 'p'},
                                    {"mod", required_argument, 0, 'm'},
                                    {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
    switch (c) {
    case 'k':
      k = atoll(optarg);
      break;
    case 'p':
      pnum = atoll(optarg);
      break;
    case 'm':
      mod = atoll(optarg);
      break;
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (k < 0 || pnum <= 0 || mod <= 0) {
    usage(argv[0]);
    return 1;
  }
  /* Чтобы произведение двух остатков помещалось в unsigned long long */
  if (mod > 4294967295LL) {
    fprintf(stderr, "mod must be <= 4294967295\n");
    return 1;
  }

  result = 1 % mod;

  pthread_t *threads = malloc(pnum * sizeof(pthread_t));
  struct thread_args *args = malloc(pnum * sizeof(struct thread_args));
  if (!threads || !args) {
    perror("malloc");
    return 1;
  }

  /* Делим множители 1..k между потоками примерно поровну */
  long long chunk = k / pnum;
  long long rest = k % pnum;
  long long next = 1;

  for (long long i = 0; i < pnum; i++) {
    long long len = chunk + (i < rest ? 1 : 0);
    args[i].begin = next;
    args[i].end = next + len - 1; /* при len == 0 диапазон пустой */
    args[i].mod = mod;
    next += len;

    if (pthread_create(&threads[i], NULL, partial_factorial, &args[i]) != 0) {
      perror("pthread_create");
      return 1;
    }
  }

  for (long long i = 0; i < pnum; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("pthread_join");
      return 1;
    }
  }

  printf("%lld! mod %lld = %llu\n", k, mod, result);

  free(threads);
  free(args);
  return 0;
}