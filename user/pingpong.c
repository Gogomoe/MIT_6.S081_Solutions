#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p[2];
    pipe(p);


    if (fork() != 0) {
        char c1 = 0;
        write(p[0], &c1, 1);
        read(p[1], &c1, 1);

        int exit;
        wait(&exit);

        printf("%d: received pong\n", getpid());

        close(p[0]);
        close(p[1]);

    } else {
        char c2;
        read(p[1], &c2, 1);
        printf("%d: received ping\n", getpid());
        write(p[0], &c2, 1);
    }

    exit(0);
}
