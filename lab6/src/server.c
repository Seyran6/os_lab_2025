#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "pthread.h"

#include "factorial.h"
#include "netio.h"

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  fargs->result = Factorial(fargs);
  return NULL;
}

int main(int argc, char **argv) {
  int tnum = -1;
  int port = -1;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        port = atoi(optarg);
        // TODO: your code here
        if (port < 1 || port > 65535) {
          fprintf(stderr, "Bad port: %s\n", optarg);
          return 1;
        }
        break;
      case 1:
        tnum = atoi(optarg);
        // TODO: your code here
        if (tnum < 1) {
          fprintf(stderr, "tnum must be >= 1\n");
          return 1;
        }
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Unknown argument\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (port == -1 || tnum == -1) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  // Клиент мог умереть до send(): без этого сервер упадёт по SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    fprintf(stderr, "Can not create server socket!");
    return 1;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
  if (err < 0) {
    fprintf(stderr, "Can not bind to socket!");
    return 1;
  }

  err = listen(server_fd, 128);
  if (err < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  printf("Server listening at %d\n", port);

  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);

    if (client_fd < 0) {
      fprintf(stderr, "Could not establish new connection\n");
      continue;
    }

    while (true) {
      unsigned int buffer_size = sizeof(uint64_t) * 3;
      char from_client[buffer_size];
      int read = RecvAll(client_fd, from_client, buffer_size);

      if (read == 0)
        break;
      if (read == -1) {
        fprintf(stderr, "Client read failed\n");
        break;
      }
      if (read == -2) {
        fprintf(stderr, "Client send wrong data format\n");
        break;
      }

      pthread_t threads[tnum];

      uint64_t begin = 0;
      uint64_t end = 0;
      uint64_t mod = 0;
      memcpy(&begin, from_client, sizeof(uint64_t));
      memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

      fprintf(stdout, "Receive: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", begin,
              end, mod);

      if (mod == 0 || begin == 0) { // деление на 0 / некорректный диапазон
        fprintf(stderr, "Client send wrong data format\n");
        break;
      }

      // Делим [begin, end] на tnum частей, последний поток берёт остаток.
      // Если чисел меньше, чем потоков (chunk == 0), первые потоки получают
      // пустой диапазон (b > e) и возвращают 1, а последний считает всё.
      uint64_t count = (end >= begin) ? end - begin + 1 : 0;
      uint64_t chunk = count / (uint64_t)tnum;

      struct FactorialArgs args[tnum];
      int created = 0;
      for (uint32_t i = 0; i < (uint32_t)tnum; i++) {
        // TODO: parallel somehow
        uint64_t b = begin + (uint64_t)i * chunk;
        args[i].begin = b;
        args[i].end = (i == (uint32_t)tnum - 1) ? end : b + chunk - 1;
        args[i].mod = mod;
        args[i].result = 1 % mod;

        if (pthread_create(&threads[i], NULL, ThreadFactorial,
                           (void *)&args[i])) {
          printf("Error: pthread_create failed!\n");
          return 1;
        }
        created++;
      }

      uint64_t total = 1 % mod;
      for (int i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
        total = MultModulo(total, args[i].result, mod);
      }

      printf("Total: %" PRIu64 "\n", total);

      char buffer[sizeof(total)];
      memcpy(buffer, &total, sizeof(total));
      if (!SendAll(client_fd, buffer, sizeof(total))) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }

  return 0;
}