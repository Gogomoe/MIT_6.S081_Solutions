#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
#include "defs.h"

uint64 sys_sysinfo(void) {
    uint64 addr;
    struct sysinfo info;

    if (argaddr(0, &addr) < 0) {
        return -1;
    }

    info.freemem = freemem();
    info.nproc = nproc();

    struct proc *p = myproc();
    if (copyout(p->pagetable, addr, (char *) &info, sizeof(info)) < 0) {
        return -1;
    }

    return 0;
}