#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "factorial.h"
#include "netio.h"

struct Server {
  char ip[255];
  int port;
};

// Задача для одного потока клиента = работа с одним сервером.
struct ClientTask {
  struct sockaddr_in addr; // адрес уже разрешён в main (gethostbyname не thread-safe)
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result; // ответ сервера
  bool spawned;    // был ли запущен поток (пустой диапазон не отправляем)
  bool ok;         // успешно ли завершилась работа с сервером
};

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0; // иначе можно поймать старую ошибку из errno
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0)
    return false;

  // strtoull молча принимает "-5" (превращает в большое число) и "12abc"
  if (end == str || *end != '\0' || strchr(str, '-') != NULL) {
    fprintf(stderr, "Not a valid unsigned number: %s\n", str);
    return false;
  }

  *val = i;
  return true;
}

// Читает файл со строками "ip:port". Возвращает массив (нужно free) и число.
static struct Server *ReadServers(const char *path, unsigned int *count) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    fprintf(stderr, "Can not open servers file %s: %s\n", path,
            strerror(errno));
    return NULL;
  }

  struct Server *list = NULL;
  unsigned int n = 0;
  char line[512];

  while (fgets(line, sizeof(line), f) != NULL) {
    // обрезаем пробелы и перевод строки справа
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                       line[len - 1] == ' ' || line[len - 1] == '\t'))
      line[--len] = '\0';
    if (len == 0)
      continue; // пустая строка

    char *colon = strrchr(line, ':');
    if (colon == NULL || colon == line || colon - line >= (long)sizeof(list->ip)) {
      fprintf(stderr, "Bad server line (expected ip:port): %s\n", line);
      free(list);
      fclose(f);
      return NULL;
    }

    char *end = NULL;
    long port = strtol(colon + 1, &end, 10);
    if (end == colon + 1 || *end != '\0' || port < 1 || port > 65535) {
      fprintf(stderr, "Bad port in line: %s\n", line);
      free(list);
      fclose(f);
      return NULL;
    }

    struct Server *tmp = realloc(list, sizeof(struct Server) * (n + 1));
    if (tmp == NULL) {
      fprintf(stderr, "Out of memory\n");
      free(list);
      fclose(f);
      return NULL;
    }
    list = tmp;

    memset(&list[n], 0, sizeof(struct Server));
    memcpy(list[n].ip, line, (size_t)(colon - line));
    list[n].port = (int)port;
    n++;
  }

  fclose(f);
  *count = n;
  return list;
}

// Поток: соединяется со своим сервером, отправляет задачу, ждёт ответ.
static void *ServerWorker(void *arg) {
  struct ClientTask *t = (struct ClientTask *)arg;
  t->ok = false;

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&t->addr, sizeof(t->addr)) < 0) {
    fprintf(stderr, "Connection failed\n");
    close(sck);
    return NULL;
  }

  // TODO: for one server
  // parallel between servers
  char task[sizeof(uint64_t) * 3];
  memcpy(task, &t->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &t->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &t->mod, sizeof(uint64_t));

  if (!SendAll(sck, task, sizeof(task))) {
    fprintf(stderr, "Send failed\n");
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (RecvAll(sck, response, sizeof(response)) != 1) {
    fprintf(stderr, "Recieve failed\n");
    close(sck);
    return NULL;
  }

  memcpy(&t->result, response, sizeof(uint64_t));
  t->ok = true;
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers[255] = {'\0'}; // TODO: explain why 255
  // 255 = NAME_MAX, максимальная длина имени файла в большинстве ФС;
  // для длинных путей лучше PATH_MAX (4096), но для лабораторной хватает.

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        // TODO: your code here
        if (!ConvertStringToUI64(optarg, &k))
          return 1;
        break;
      case 1:
        // TODO: your code here
        if (!ConvertStringToUI64(optarg, &mod))
          return 1;
        break;
      case 2:
        // TODO: your code here
        if (strlen(optarg) >= sizeof(servers)) { // иначе переполнение буфера
          fprintf(stderr, "Servers file path is too long\n");
          return 1;
        }
        memcpy(servers, optarg, strlen(optarg));
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == -1 || mod == -1 || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  if (mod == 0) {
    fprintf(stderr, "mod must be greater than 0\n");
    return 1;
  }

  // Если сервер оборвёт соединение, send() не должен убивать клиент сигналом.
  signal(SIGPIPE, SIG_IGN);

  // TODO: for one server here, rewrite with servers from file
  unsigned int servers_num = 0;
  struct Server *to = ReadServers(servers, &servers_num);
  if (to == NULL)
    return 1;
  if (servers_num == 0) {
    fprintf(stderr, "No servers in file %s\n", servers);
    free(to);
    return 1;
  }

  struct ClientTask *tasks = calloc(servers_num, sizeof(struct ClientTask));
  pthread_t *threads = malloc(sizeof(pthread_t) * servers_num);
  if (tasks == NULL || threads == NULL) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }

  // TODO: delete this and parallel work between servers
  // Делим диапазон [1, k] на servers_num почти равных частей,
  // последний сервер забирает остаток.
  uint64_t chunk = k / servers_num;

  for (unsigned int i = 0; i < servers_num; i++) {
    struct hostent *hostname = gethostbyname(to[i].ip);
    if (hostname == NULL) {
      fprintf(stderr, "gethostbyname failed with %s\n", to[i].ip);
      exit(1);
    }

    memset(&tasks[i].addr, 0, sizeof(tasks[i].addr));
    tasks[i].addr.sin_family = AF_INET;
    tasks[i].addr.sin_port = htons(to[i].port);
    // копируем h_length (4) байта, а не читаем unsigned long (8 байт на x86_64)
    memcpy(&tasks[i].addr.sin_addr.s_addr, hostname->h_addr,
           sizeof(tasks[i].addr.sin_addr.s_addr));

    tasks[i].begin = (uint64_t)i * chunk + 1;
    tasks[i].end = (i == servers_num - 1) ? k : (uint64_t)(i + 1) * chunk;
    tasks[i].mod = mod;
    tasks[i].result = 1 % mod;
    tasks[i].ok = true;
  }

  // TODO: work continiously, rewrite to make parallel
  // Запускаем по одному потоку на сервер: все сервера считают одновременно.
  for (unsigned int i = 0; i < servers_num; i++) {
    if (tasks[i].begin > tasks[i].end) // пустой кусок (k < servers_num)
      continue;

    if (pthread_create(&threads[i], NULL, ServerWorker, &tasks[i]) != 0) {
      fprintf(stderr, "pthread_create failed\n");
      exit(1);
    }
    tasks[i].spawned = true;
  }

  // TODO: from one server
  // unite results
  uint64_t answer = 1 % mod;
  bool failed = false;
  for (unsigned int i = 0; i < servers_num; i++) {
    if (tasks[i].spawned)
      pthread_join(threads[i], NULL);

    if (!tasks[i].ok) {
      fprintf(stderr, "Server %s:%d failed\n", to[i].ip, to[i].port);
      failed = true;
      continue;
    }
    answer = MultModulo(answer, tasks[i].result, mod);
  }

  free(threads);
  free(tasks);
  free(to);

  if (failed)
    return 1;

  printf("answer: %" PRIu64 "\n", answer);
  return 0;
}