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
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  struct spinlock table_lock[NBUCKET];
  struct buf table[NBUCKET];
} bcache;

void lockAll(){
  for(int i=0; i<NBUCKET; ++i){
      acquire(&bcache.table_lock[i]);
  }
}

void releaseAll(){
  for(int i=0; i<NBUCKET; ++i){
      release(&bcache.table_lock[i]);
  }
}

void
binit(void)
{
  struct buf *b;
  char name[10];

  initlock(&bcache.lock, "bcache");
  for(int i=0; i<NBUCKET; ++i){
    snprintf(name, 10, "bcache_%d", i);
    initlock(&bcache.table_lock[i], name);
    bcache.table[i].next = &bcache.table[i];
    bcache.table[i].prev = &bcache.table[i];
  }

  int i = 0;
  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.table[i].next;
    b->prev = &bcache.table[i];
    initsleeplock(&b->lock, "buffer");
    bcache.table[i].next->prev = b;
    bcache.table[i].next = b;
    i = (i+1) % NBUCKET;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
    struct buf *b, *lru_b = 0;
    int bucket = blockno % NBUCKET;
    int min_ticks = 2147483647;

    acquire(&bcache.table_lock[bucket]);

    // Is the block already cached?
    for(b = bcache.table[bucket].next; b != &bcache.table[bucket]; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
            b->refcnt++;
            release(&bcache.table_lock[bucket]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Find LRU in own bucket first
    for(int i=0; i<NBUCKET; ++i){
        int t = (bucket+i) % NBUCKET;
        if(t != bucket)
            acquire(&bcache.table_lock[t]);
        for(b = bcache.table[t].next; b!= &bcache.table[t]; b = b->next){
            if(b->refcnt == 0 && b->lru < min_ticks){
                lru_b = b;
                min_ticks = b->lru;
            }
        }
        if(lru_b){
            lru_b->dev = dev;
            lru_b->blockno = blockno;
            lru_b->valid = 0;
            lru_b->refcnt = 1;
            if(t != bucket){
                lru_b->next->prev = lru_b->prev;
                lru_b->prev->next = lru_b->next;
                lru_b->next = bcache.table[bucket].next;
                lru_b->prev = &bcache.table[bucket];
                bcache.table[bucket].next->prev = lru_b;
                bcache.table[bucket].next = lru_b;
                release(&bcache.table_lock[t]);
            }
            release(&bcache.table_lock[bucket]);
            acquiresleep(&lru_b->lock);
            return lru_b;
        }
        if(t != bucket)
            release(&bcache.table_lock[t]);
    }


    // Version 2
    // Find LRU in all bufs
    // release(&bcache.table_lock[bucket]);
    // // Not cached.
    // // Recycle the least recently used (LRU) unused buffer.
    // acquire(&bcache.lock);
    // lockAll();
    // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    //     if(b->dev == dev && b->blockno == blockno){
    //         b->refcnt++;
    //         releaseAll();
    //         release(&bcache.lock);
    //         acquiresleep(&b->lock);
    //         return b;
    //     }else if(b->refcnt == 0 && b->lru < min_ticks){
    //         lru_b = b;
    //         min_ticks = b->lru;
    //     }
    // }
    // if(lru_b){
    //     if(lru_b->blockno % NBUCKET != bucket){
    //         lru_b->next->prev = lru_b->prev;
    //         lru_b->prev->next = lru_b->next;
    //         lru_b->next = bcache.table[bucket].next;
    //         lru_b->prev = &bcache.table[bucket];
    //         bcache.table[bucket].next->prev = lru_b;
    //         bcache.table[bucket].next = lru_b;
    //     }
    //     lru_b->dev = dev;
    //     lru_b->blockno = blockno;
    //     lru_b->valid = 0;
    //     lru_b->refcnt = 1;
    //     releaseAll();
    //     release(&bcache.lock);
    //     acquiresleep(&lru_b->lock);
    //     return lru_b;
    // }
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.table_lock[bucket]);
  b->refcnt--;
  if(b->refcnt == 0){
     b->lru = ticks;
  }
  release(&bcache.table_lock[bucket]);
}

void
bpin(struct buf *b) {
  //acquire(&bcache.lock);
  acquire(&bcache.table_lock[b->blockno % NBUCKET]);
  b->refcnt++;
  release(&bcache.table_lock[b->blockno % NBUCKET]);
  //release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  //acquire(&bcache.lock);
  acquire(&bcache.table_lock[b->blockno % NBUCKET]);
  b->refcnt--;
  release(&bcache.table_lock[b->blockno % NBUCKET]);
  //release(&bcache.lock);
}


