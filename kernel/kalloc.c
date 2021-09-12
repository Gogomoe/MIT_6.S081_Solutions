// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
    uint64 count;
    char name[20];
} kmem[NCPU];

struct spinlock steal;

void
kinit() {
    initlock(&steal, "steal_kmem");
    for (int i = 0; i < NCPU; ++i) {
        snprintf(kmem[i].name, 20, "kmem_%d", i);
        initlock(&kmem[i].lock, kmem[i].name);
        kmem[i].count = 0;
    }

    freerange(end, (void *) PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *) PGROUNDUP((uint64) pa_start);
    for (; p + PGSIZE <= (char *) pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa) {
    struct run *r;

    if (((uint64) pa % PGSIZE) != 0 || (char *) pa < end || (uint64) pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *) pa;

    push_off();
    acquire(&kmem[cpuid()].lock);

    r->next = kmem[cpuid()].freelist;
    kmem[cpuid()].freelist = r;
    kmem[cpuid()].count++;

    release(&kmem[cpuid()].lock);
    pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
    struct run *r;

    push_off();
    acquire(&kmem[cpuid()].lock);

    if (kmem[cpuid()].count == 0) {
        acquire(&steal);

        uint64 max_count = 0;
        uint64 max_cpu = cpuid();
        for (int i = 0; i < NCPU; ++i) {
            if (i == cpuid()) {
                continue;
            }
            if (kmem[i].count > max_count && kmem[i].count > 0) {
                max_count = kmem[i].count;
                max_cpu = i;
            }
        }

        // it may deadlock
        // 1. cpu0 steal, found cpu1 have 1 free page
        // 2. cpu1 kalloc, remain 0 free page
        // 3. cpu1 kalloc, acquire lock of cpu1, try to steal
        // 4. cpu0 try to acquire lock of cpu1, cpu1 try to acquire lock of steal

        if (max_count != 0 && max_cpu != cpuid()) {
            acquire(&kmem[max_cpu].lock);

            if (kmem[max_cpu].count > 0) {
                while (kmem[cpuid()].count < kmem[max_cpu].count) {
                    struct run *s = kmem[max_cpu].freelist;
                    kmem[max_cpu].freelist = s->next;
                    s->next = kmem[cpuid()].freelist;
                    kmem[cpuid()].freelist = s;
                    kmem[max_cpu].count--;
                    kmem[cpuid()].count++;
                }
            }

            release(&kmem[max_cpu].lock);
        }

        release(&steal);
    }

    r = kmem[cpuid()].freelist;
    if (r) {
        kmem[cpuid()].freelist = r->next;
        kmem[cpuid()].count--;
    }

    release(&kmem[cpuid()].lock);
    pop_off();

    if (r)
        memset((char *) r, 5, PGSIZE); // fill with junk
    return (void *) r;
}
