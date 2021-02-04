#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int tickets;

    if (argc < 2) {
        printf("sleep <n> \n");
        exit(1);
    }

    tickets = atoi(argv[1]);

    printf("sleep for %d tickets\n", tickets);
    sleep(tickets);

    exit(0);
}
