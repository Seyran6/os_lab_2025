#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define N 3

static void show_children(const char *title)
{
    char cmd[64];
    printf("\n%s\n", title);
    fflush(stdout);
    snprintf(cmd, sizeof cmd, "ps -o pid,ppid,stat,cmd --ppid %d", (int)getpid());
    system(cmd);
}

int main(void)
{
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid == 0)
            _exit(0);              
        printf("Создан потомок PID=%d\n", (int)pid);
    }

    sleep(1);                       
    show_children("До wait(): потомки-зомби (STAT = Z)");

    for (int i = 0; i < N; i++)
        wait(NULL);                 

    show_children("После wait(): зомби нет");
    return 0;
}