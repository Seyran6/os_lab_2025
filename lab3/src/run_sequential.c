#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: %s seed arraysize\n", argv[0]);
    return 1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    char *args[] = {"./sequential_min_max", argv[1], argv[2], NULL};
    execv(args[0], args);

    perror("execv");
    _exit(1);
  }

  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    printf("sequential_min_max finished with exit code %d\n", WEXITSTATUS(status));
    return WEXITSTATUS(status);
  }

  return 0;
}