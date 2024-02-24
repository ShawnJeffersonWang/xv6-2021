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

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf head;
} bcache;

#define BUCKETSIZE 13
#define BUFFERSIZE 5

extern uint ticks;

struct {
    struct spinlock lock;
    struct buf buf[BUFFERSIZE];
} bcachebucket[BUCKETSIZE];

int
hash(uint blockno) {
    return blockno % BUCKETSIZE;
}

void
bpin(struct buf *b) {
    int bucket = hash(b->blockno);
    acquire(&bcachebucket[bucket].lock);
    b->refcnt++;
    release(&bcachebucket[bucket].lock);
}

void
bunpin(struct buf *b) {
    int bucket = hash(b->blockno);
    acquire(&bcachebucket[bucket].lock);
    b->refcnt--;
    release(&bcachebucket[bucket].lock);
}

void
binit(void) {
    struct buf *b;

    initlock(&bcache.lock, "bcache");

    // Create linked list of buffers
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

    for (int i = 0; i < BUCKETSIZE; i++) {
        initlock(&bcachebucket[i].lock, "bcachebucket");
        for (int j = 0; j < BUFFERSIZE; j++) {
            initsleeplock(&bcachebucket[i].buf[j].lock, "buffer");
        }
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno) {
    struct buf *b;
    int bucket = hash(blockno);

//    acquire(&bcache.lock);
    acquire(&bcachebucket[bucket].lock);
    // Is the block already cached?
//    for (b = bcache.head.next; b != &bcache.head; b = b->next) {
//        if (b->dev == dev && b->blockno == blockno) {
//            b->refcnt++;
//            release(&bcache.lock);
//            acquiresleep(&b->lock);
//            return b;
//        }
//    }
//
//    // Not cached.
//    // Recycle the least recently used (LRU) unused buffer.
//    for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
//        if (b->refcnt == 0) {
//            b->dev = dev;
//            b->blockno = blockno;
//            b->valid = 0;
//            b->refcnt = 1;
//            release(&bcache.lock);
//            acquiresleep(&b->lock);
//            return b;
//        }
//    }
//    panic("bget: no buffers");
    for (int i = 0; i < BUFFERSIZE; i++) {
        b = &bcachebucket[bucket].buf[i];
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            b->lastuse = ticks;
            release(&bcachebucket[bucket].lock);
            acquiresleep(&b->lock);
            return b;
        }
    }
    uint least = 0xffffffff;
    int least_idx = -1;
    for (int i = 0; i < BUFFERSIZE; i++) {
        b = &bcachebucket[bucket].buf[i];
        if (b->refcnt == 0 && b->lastuse < least) {
            least = b->lastuse;
            least_idx = i;
        }
    }

    if (least_idx == -1) {
        panic("bget: no unused buffer for recycle");
    }

    b = &bcachebucket[bucket].buf[least_idx];
    b->dev = dev;
    b->blockno = blockno;
    b->lastuse = ticks;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcachebucket[bucket].lock);
    acquiresleep(&b->lock);
    return b;
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

//    releasesleep(&b->lock);
//
//    acquire(&bcache.lock);
//    b->refcnt--;
//    if (b->refcnt == 0) {
//        // no one is waiting for it.
//        b->next->prev = b->prev;
//        b->prev->next = b->next;
//        b->next = bcache.head.next;
//        b->prev = &bcache.head;
//        bcache.head.next->prev = b;
//        bcache.head.next = b;
//    }
//
//    release(&bcache.lock);
    int bucket = hash(b->blockno);
    acquire(&bcachebucket[bucket].lock);
    b->refcnt--;
    release(&bcachebucket[bucket].lock);
    releasesleep(&b->lock);
}

//void
//bpin(struct buf *b) {
//    acquire(&bcache.lock);
//    b->refcnt++;
//    release(&bcache.lock);
//}
//
//void
//bunpin(struct buf *b) {
//    acquire(&bcache.lock);
//    b->refcnt--;
//    release(&bcache.lock);
//}


