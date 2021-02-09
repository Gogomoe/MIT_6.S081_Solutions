#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static const int READ = 0;
static const int WRITE = 1;

int main(int argc, char *argv[]) {
    int p1[2];
    pipe(p1);
    if (fork() != 0) {
        close(p1[READ]);

        for (char i = 2; i <= 35; i++) {
            write(p1[WRITE], &i, 1);
        }
        close(p1[WRITE]);

        int exit;
        wait(&exit);
    } else {
        int have_child = 0;
        int p2[2];
        char prime;
        char buf;

        INIT_SUBPROCESS:
        close(p1[WRITE]);
        read(p1[READ], &prime, 1);
        printf("prime %d\n", prime);

        while (read(p1[READ], &buf, 1) != 0) {
            if (buf % prime != 0) {
                if (!have_child) {
                    pipe(p2);
                    if (fork() == 0) {
                        close(p1[READ]);
                        p1[READ] = p2[READ];
                        p1[WRITE] = p2[WRITE];
                        have_child = 0;
                        goto INIT_SUBPROCESS;
                    } else {
                        have_child = 1;
                        close(p2[READ]);
                    }
                }
                write(p2[WRITE], &buf, 1);
            }
        }

        close(p1[READ]);

        if (have_child == 1) {
            close(p2[WRITE]);

            int exit;
            wait(&exit);
        }
    }

    exit(0);
}
