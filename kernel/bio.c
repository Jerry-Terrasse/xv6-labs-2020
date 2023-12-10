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

#define HASHMAGIC 0xdeadbeef
#define HASHMASK 0x7
#define NHASH 8
#define NBUF_PER (NBUF)

struct {
  struct spinlock lock;
  struct buf buf[NBUF_PER];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcaches[NHASH];

void
binit(void)
{
  struct buf *b;
  static char locknames[NHASH][9];

  for(int i=0; i<NHASH; ++i) {
    memmove(locknames[i], "bcache_a", 9);
    locknames[i][7] = 'a' + i;
    initlock(&bcaches[i].lock, locknames[i]);
    bcaches[i].head.prev = &bcaches[i].head;
    bcaches[i].head.next = &bcaches[i].head;
    // Create linked list of buffers
    for(b = bcaches[i].buf; b < bcaches[i].buf+NBUF_PER; b++){
      b->next = bcaches[i].head.next;
      b->prev = &bcaches[i].head;
      initsleeplock(&b->lock, "buffer");
      bcaches[i].head.next->prev = b;
      bcaches[i].head.next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hid = (blockno ^ HASHMAGIC) & HASHMASK;

  acquire(&bcaches[hid].lock);

  // Is the block already cached?
  for(b = bcaches[hid].head.next; b != &bcaches[hid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcaches[hid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcaches[hid].head.prev; b != &bcaches[hid].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcaches[hid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // steal from aother hash bucket
  release(&bcaches[hid].lock);
  struct buf *new_b = 0;
  for(int i = (hid+1)&HASHMASK; i != hid; i = (i+1)&HASHMASK) {
    acquire(&bcaches[i].lock);
    for(b = bcaches[i].head.prev; b != &bcaches[i].head; b = b->prev){
      if(b->refcnt == 0) {
        new_b = b;
        b->prev->next = b->next;
        b->next->prev = b->prev;
        // bcaches[i].head.next->prev = b->prev;
        // bcaches[i].head.next = b->next;
        break;
      }
    }
    release(&bcaches[i].lock);
  }
  if(new_b) {
    acquire(&bcaches[hid].lock);
    new_b->dev = dev;
    new_b->blockno = blockno;
    new_b->valid = 0;
    new_b->refcnt = 1;
    new_b->next = bcaches[hid].head.next;
    new_b->prev = &bcaches[hid].head;
    bcaches[hid].head.next->prev = new_b;
    bcaches[hid].head.next = new_b;
    release(&bcaches[hid].lock);
    acquiresleep(&new_b->lock);
    return new_b;
  }
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

  int hid = (b->blockno ^ HASHMAGIC) & HASHMASK;

  acquire(&bcaches[hid].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcaches[hid].head.next;
    b->prev = &bcaches[hid].head;
    bcaches[hid].head.next->prev = b;
    bcaches[hid].head.next = b;
  }
  
  release(&bcaches[hid].lock);
}

void
bpin(struct buf *b) {
  int hid = (b->blockno ^ HASHMAGIC) & HASHMASK;
  acquire(&bcaches[hid].lock);
  b->refcnt++;
  release(&bcaches[hid].lock);
}

void
bunpin(struct buf *b) {
  int hid = (b->blockno ^ HASHMAGIC) & HASHMASK;
  acquire(&bcaches[hid].lock);
  b->refcnt--;
  release(&bcaches[hid].lock);
}