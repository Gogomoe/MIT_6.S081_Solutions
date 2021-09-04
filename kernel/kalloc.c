// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#define PA2REFI(pa) (((pa) - KERNBASE) / PGSIZE)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

void kfree_force(void *pa);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;

unsigned char page_ref[PHYSIZE / PGSIZE];

void
kinit() {
    initlock(&kmem.lock, "kmem");
    freerange(end, (void *) PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *) PGROUNDUP((uint64) pa_start);
    for (; p + PGSIZE <= (char *) pa_end; p += PGSIZE)
        kfree_force(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa) {
    if (((uint64) pa % PGSIZE) != 0 || (char *) pa < end || (uint64) pa >= PHYSTOP)
        panic("kfree");

    if (page_ref[PA2REFI((uint64) pa)] == 0) {
        panic("kfree ref");
    }
    decrease_ref((uint64) pa);
    if (page_ref[PA2REFI((uint64) pa)] != 0) {
        return;
    }

    kfree_force(pa);
}

void kfree_force(void *pa) {
    struct run *r;

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *) pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if (r) {
        memset((char *) r, 5, PGSIZE); // fill with junk

        uint64 pa = (uint64) r;
        if (page_ref[PA2REFI(pa)] != 0) {
            panic("kalloc ref");
        }
        increase_ref(pa);
    }
    return (void *) r;
}

void increase_ref(uint64 pa) {
    if (page_ref[PA2REFI(pa)] == 255) {
        panic("increase_ref");
    }
    page_ref[PA2REFI(pa)]++;
}

void decrease_ref(uint64 pa) {
    if (page_ref[PA2REFI(pa)] == 0) {
        panic("decrease_ref");
    }
    page_ref[PA2REFI(pa)]--;
}