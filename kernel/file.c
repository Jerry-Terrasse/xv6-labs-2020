//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "fcntl.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

struct {
  struct spinlock lock;
  struct VMA vmas[TOTVMA];
} vmas;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
  initlock(&vmas.lock, "VMA");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

void*
mmap(uint length, int prot, int flags, struct file *f)
{
  void* const MAP_FAILED = (void*)-1;
  struct VMA *vma;
  struct proc *p = myproc();

  if(length == 0){
    return MAP_FAILED;
  }

  uint64 used = 0x3000000000;
  for(int i=0; i < NVMA; ++i) {
    vma = p->vma[i];
    if(vma == 0){
      continue;
    }
    used = vma->start < used ? vma->start : used;
  }
  used = PGROUNDDOWN(used);
  uint64 start = PGROUNDDOWN(used - length);
  if(start < p->sz){
    return MAP_FAILED;
  }
  if(start + length > used){
    return MAP_FAILED;
  }

  int idx;
  for(idx=0; idx < NVMA; ++idx){
    if(p->vma[idx] == 0){
      break;
    }
  }
  if(idx == NVMA){ // reach process's max VMA
    return MAP_FAILED;
  }

  acquire(&vmas.lock);
  for(vma = vmas.vmas; vma < vmas.vmas + TOTVMA; vma++){
    if(vma->file == 0){
      break;
    }
  }
  if(vma == vmas.vmas + TOTVMA){ // reach system's max VMA
    return MAP_FAILED;
  }
  vma->file = filedup(f); // mark as in use
  release(&vmas.lock);

  p->vma[idx] = vma;
  vma->prot = prot;
  vma->flags = flags;
  vma->len = length;
  vma->start = start;
  vma->unmaped_len = 0;
  return (void*)start;
}

int
handle_page_fault(uint64 va)
{
  printf("handle_page_fault: va=%p\n", va);
  struct proc *p = myproc();
  struct VMA *vma = 0;

  for(int i=0; i < NVMA; ++i) {
    if(p->vma[i] == 0) {
      continue;
    }
    if(p->vma[i]->start <= va && va < p->vma[i]->start + p->vma[i]->len) {
      vma = p->vma[i];
      break;
    }
  }
  if(vma == 0){
    return -1;
  }

  void* pa = kalloc();
  if(pa == 0){
    return -1;
  }
  memset(pa, 0x00, PGSIZE);
  if(readi(vma->file->ip, 0, (uint64)pa, PGROUNDDOWN(va) - vma->start, PGSIZE) == -1){
    kfree(pa);
    return -1;
  }
  int perm = PTE_U;
  if(vma->prot & PROT_READ){
    perm |= PTE_R;
  }
  if(vma->prot & PROT_WRITE){
    perm |= PTE_W;
  }
  if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)pa, perm) != 0){
    kfree(pa);
    return -1;
  }

  return 0;
}

int munmap(uint64 addr, uint length)
{
  struct proc *p = myproc();
  struct VMA *vma = 0;
  int idx;

  for(idx=0; idx < NVMA; ++idx) {
    if(p->vma[idx] == 0) {
      continue;
    }
    if(p->vma[idx]->start <= addr && addr < p->vma[idx]->start + p->vma[idx]->len) {
      vma = p->vma[idx];
      break;
    }
  }
  if(vma == 0){
    return -1;
  }

  for(int offset = 0; offset < length; offset += PGSIZE){
    uint64 va = (uint64)addr + offset;
    uint64 pa = walkaddr(p->pagetable, va);
    if(pa == 0){
      continue;
    }
    if((vma->prot & PROT_WRITE) && vma->flags == MAP_SHARED){
      begin_op();
      ilock(vma->file->ip);
      if(writei(vma->file->ip, 0, pa, va - vma->start, PGSIZE) == -1){
        iunlock(vma->file->ip);
        end_op();
        return -1;
      }
      iunlock(vma->file->ip);
      end_op();
    }
    printf("munmap: va=%p, pa=%p\n", va, pa);
    uvmunmap(p->pagetable, va, 1, 1);
  }

  vma->unmaped_len += length;
  if(vma->unmaped_len >= vma->len){ // only work in this lab
    fileclose(vma->file);
    acquire(&vmas.lock);
    vma->file = 0;
    release(&vmas.lock);
    p->vma[idx] = 0;
  }
  

  return 0;
}