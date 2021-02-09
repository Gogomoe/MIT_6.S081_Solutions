#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void run_command(char *file, char *args[]) {
    if (fork() == 0) {
        exec(file, args);
    }
}

int main(int argc, char *argv[]) {
    char ext_arg[500];
    char *args[argc + 1];

    char *file = argv[1];

    for (int i = 0; i < argc - 1; ++i) {
        args[i] = argv[i + 1];
    }
    args[argc - 1] = ext_arg;
    args[argc] = 0;

    char buf[500];
    int size;
    int p = 0;

    while ((size = read(0, buf, 500)) != 0) {
        for (int i = 0; i < size; ++i) {
            if (buf[i] != '\n') {
                ext_arg[p++] = buf[i];
                continue;
            } else {
                ext_arg[p] = 0;
                p = 0;
                run_command(file, args);

            }
        }
    }
    if (p != 0) {
        ext_arg[p] = 0;
        p = 0;
        run_command(file, args);
    }

    exit(0);
}
