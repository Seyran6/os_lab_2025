#include "netio.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

bool SendAll(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, buf + sent, len - sent, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    sent += (size_t)n;
  }
  return true;
}

int RecvAll(int fd, char *buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    ssize_t n = recv(fd, buf + got, len - got, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      return got == 0 ? 0 : -2;
    got += (size_t)n;
  }
  return 1;
}