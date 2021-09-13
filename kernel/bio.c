// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
    struct spinlock lock[NBUCKET];
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf head[NBUCKET];
} bcache;

struct spinlock steal_bio;

void
binit(void) {
    struct buf *b;

    initlock(&steal_bio, "steal_bio");

    for (int i = 0; i < NBUCKET; ++i) {
        initlock(&bcache.lock[i], "bcache");
        bcache.head[i].prev = &bcache.head[i];
        bcache.head[i].next = &bcache.head[i];
    }

    // Create linked list of buffers

    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.head[0].next;
        b->prev = &bcache.head[0];
        initsleeplock(&b->lock, "buffer");
        bcache.head[0].next->prev = b;
        bcache.head[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno) {
    struct buf *b;

    uint hash = blockno % NBUCKET;
    acquire(&bcache.lock[hash]);

    // Is the block already cached?
    for (b = bcache.head[hash].next; b != &bcache.head[hash]; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock[hash]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    for (b = bcache.head[hash].prev; b != &bcache.head[hash]; b = b->prev) {
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock[hash]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    release(&bcache.lock[hash]);
    acquire(&steal_bio);
    acquire(&bcache.lock[hash]);

    uint target = (hash + 1) % NBUCKET;
    while (target != hash) {
        acquire(&bcache.lock[target]);

        int found = 0;
        for (b = bcache.head[target].prev; b != &bcache.head[target]; b = b->prev) {
            if (b->refcnt == 0) {
                b->prev->next = b->next;
                b->next->prev = b->prev;

                b->prev = bcache.head[hash].prev;
                bcache.head[hash].prev->next = b;
                b->next = &bcache.head[hash];
                bcache.head[hash].prev = b;

                found = 1;
                break;
            }
        }

        release(&bcache.lock[target]);
        if (found) {
            break;
        }
        target = (target + 1) % NBUCKET;
    }

    for (b = bcache.head[hash].prev; b != &bcache.head[hash]; b = b->prev) {
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock[hash]);
            release(&steal_bio);
            acquiresleep(&b->lock);
            return b;
        }
    }

    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    uint hash = b->blockno % NBUCKET;

    acquire(&bcache.lock[hash]);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head[hash].next;
        b->prev = &bcache.head[hash];
        bcache.head[hash].next->prev = b;
        bcache.head[hash].next = b;
    }

    release(&bcache.lock[hash]);
}

void
bpin(struct buf *b) {
    uint hash = b->blockno % NBUCKET;

    acquire(&bcache.lock[hash]);
    b->refcnt++;
    release(&bcache.lock[hash]);
}

void
bunpin(struct buf *b) {
    uint hash = b->blockno % NBUCKET;

    acquire(&bcache.lock[hash]);
    b->refcnt--;
    release(&bcache.lock[hash]);
}


