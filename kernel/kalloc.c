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
// void freerange_cpu(void *pa_start, void *pa_end, int cid);
// void kfree_cpu(void *pa, int cid);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];

void
kinit()
{
  static char buf[NCPU][7];
  // uint64 mem_per_cpu = (PHYSTOP - (uint64)end) / NCPU / PGSIZE * PGSIZE;
  // char *cpu_mem_start = end, *cpu_mem_end = 0;
  // cpu_mem_start = (char*)PGROUNDUP((uint64)cpu_mem_start);
  // cpu_mem_end = cpu_mem_start + mem_per_cpu;
  // printf("cpu_mem_start = %p\n", cpu_mem_start);
  // printf("end = %p\n", end);
  // for(int i=0; i<NCPU; ++i) {
  //   memmove(buf[i], "kmem_a", 7);
  //   buf[i][5] = 'a' + i;
  //   initlock(&kmems[i].lock, buf[i]);
  //   kmems[i].freelist = (struct run*)cpu_mem_start;
  //   freerange_cpu(cpu_mem_start, i == NCPU-1 ? (void*)PHYSTOP : cpu_mem_end, i);
  //   cpu_mem_start += mem_per_cpu;
  //   cpu_mem_end += mem_per_cpu;
  // }

  for(int i=0; i<NCPU; ++i) {
    memmove(buf[i], "kmem_a", 7);
    buf[i][5] = 'a' + i;
    initlock(&kmems[i].lock, buf[i]);
    kmems[i].freelist = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// void freerange_cpu(void *pa_start, void *pa_end, int cid)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint64)pa_start);
//   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
//     kfree_cpu(p, cid);
// }

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  push_off();
  int cid = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
  pop_off();
}

// void
// kfree_cpu(void *pa, int cid)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree_cpu");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;

//   acquire(&kmems[cid].lock);
//   r->next = kmems[cid].freelist;
//   kmems[cid].freelist = r;
//   release(&kmems[cid].lock);
// }

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  int cid = cpuid();

  acquire(&kmems[cid].lock);
  r = kmems[cid].freelist;
  if(r)
    kmems[cid].freelist = r->next;
  release(&kmems[cid].lock);

  if(!r) {
    // try steal from other cpu
    for(int i=(cid+NCPU-1)%NCPU; i!=cid; i=(i+NCPU-1)%NCPU) {
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if(r) {
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
