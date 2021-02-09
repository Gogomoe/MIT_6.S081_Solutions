#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


char *
basename(char *path) {
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--);
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    strcpy(buf, p);
    return buf;
}

void
find(char *path, char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_FILE: {
            if (strcmp(basename(path), name) == 0) {
                printf("%s\n", path);
            }
            break;
        }
        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }

                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                find(buf, name);
            }
            break;
    }
    close(fd);
}

int
main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("use 'find <path> name'\n");
    } else {
        find(argv[1], argv[2]);
    }
    exit(0);
}
