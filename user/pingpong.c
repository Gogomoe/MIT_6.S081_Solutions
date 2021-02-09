#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);

    if (fork() != 0) {
        close(p1[0]);
        close(p2[1]);

        char c1 = 0;
        write(p1[1], &c1, 1);
        read(p2[0], &c1, 1);

        int exit;
        wait(&exit);

        printf("%d: received pong\n", getpid());

        close(p1[1]);
        close(p2[0]);

    } else {
        close(p1[1]);
        close(p2[0]);

        char c2;
        read(p1[0], &c2, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], &c2, 1);

        close(p2[1]);
        close(p1[0]);

    }

    exit(0);
}
