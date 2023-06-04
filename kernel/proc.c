#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.



void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

void pagesinit(struct proc *p){
  p->total_number_of_pages = 0;
  for(struct pageondisk* page = p->pagesondisk; page < &p->pagesondisk[MAX_PAGES_ON_DISK]; page++)
  {
    page->va = 0;
    page->pa = 0;
    page->fileoffset = -1;
  }
}

uint64 diskoffset_of_va(struct proc* p, uint64 va)
{
  for(struct pageondisk* page = p->pagesondisk; page < &p->pagesondisk[MAX_PAGES_ON_DISK]; page++)
  {
    if(page->va == va)
      return page->fileoffset;
  }
  return -1;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;


  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  p->swapFile = 0;
  pagesinit(p);


  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;
  

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p, p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, p->pagesondisk, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(np, p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);
  
  if(fork_memory(np) < 0){
    freeproc(np);
    return -1;
  }

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

pte_t* random_physical_page(pagetable_t pagetable, struct pageondisk* pages)
{
  pte_t* ptes[MAX_TOTAL_PAGES];
  int arrsize = 0;
  int total_pages = number_of_used_pages(pagetable);

  for(int i=0; i<total_pages; i++)
  {
    pte_t* pte = &pagetable[i];
    if(!(*pte & PTE_PG))
    {
      ptes[arrsize++] = pte;
    }
  }

  if(arrsize == 0)
    panic("random_physical_page");

  uint random_index = (uint) random_in_range(0, arrsize);

  return ptes[random_index];
}

uint number_of_physical_pages(pagetable_t pagetable, struct pageondisk* pages)
{
  uint counter = 0; 
  for(struct pageondisk* page = pages; page < &pages[MAX_PAGES_ON_DISK]; page++)
  {
    if(page->va != 0)
      counter++;
  }
  return number_of_used_pages(pagetable) - counter;
}

uint number_of_used_pages(pagetable_t pagetable)
{
  
  for(int counter = 0; counter < MAX_PAGES_ON_DISK; counter++)
  {
    if(walkaddr(pagetable, counter * PGSIZE) == 0)
      return counter;
  }
  return -1;
}

struct pageondisk* get_diskspace(struct pageondisk* pages, uint64 va)
{

  for(struct pageondisk* page = pages; page < &pages[MAX_PAGES_ON_DISK]; page++)
  {
    if(page->va == va)
      return page;
  }

  // no entry - trying to allocate
  for(int i=0; i<MAX_PAGES_ON_DISK;i++)
  {
    struct pageondisk* page = &pages[i];
    if(page->va == 0)
    {
      page->va = va;
      page->fileoffset = i * PGSIZE;
      return page;
    } 
  }

  return 0;
}



uint64 move_random_page_to_disk(pagetable_t pagetable, struct pageondisk *pages) 
{
  pte_t* pte = random_physical_page(pagetable, pages);
  uint64 pa = PTE2PA(*pte);
  struct pageondisk* diskpage = get_diskspace(pages, (uint64)pte);
  
  if(diskpage == 0) {
    panic("vmem file is full!");
  }
  diskpage->pa = pa;
  diskpage->flags = PTE_FLAGS(*pte);

  uint64 offset = diskpage->fileoffset;

  lazy_write_to_swapfile(myproc(), (char*) pa, diskpage->fileoffset, PGSIZE);

  *pte |= PTE_PG;
  *pte &= ~PTE_V;

  return PTE2PA(*pte);
}



pte_t* last_used_page()
{
  struct proc* p = myproc();
  return &p->pagetable[p->total_number_of_pages - 1];
}

struct proc* proc_of_pte(pagetable_t pagetable)
{
  for(struct proc* p = proc; p<&proc[NPROC]; p++)
  {
    if(p->pagetable == pagetable)
      return p;
  }

  return 0;
}

uint64 add_page(struct pageondisk* pages, pagetable_t pagetable, uint64 size, int a, int xperm, int oldsz)
{
  if(pages == 0)
    panic("add_page: cannot add to NULL table!");

  if(number_of_used_pages(pages) == MAX_TOTAL_PAGES) 
    return 0;
  
  uint64 mem;
  if(number_of_physical_pages(pagetable, pages) < MAX_PSYC_PAGES){
    mem = (uint64) kalloc();
  } else {
    mem = move_random_page_to_disk(pagetable, pages);
  }

  if(mem == 0){
     uvmdealloc(pagetable, a, PGROUNDUP(oldsz));
    return 0;
  }

  if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
    kfree(mem);
    uvmdealloc(pagetable, a, PGROUNDUP(oldsz));
    return 0;
  }
  memset(mem, 0, PGSIZE);



  // printf("pid: %d: adding page at %p\n", p->pid, mem);
  

  return mem;
}

pte_t *
superwalk(struct proc* p, uint64 va)
{
  pte_t* walkres = walk(p->pagetable, va, 0);
  if(walkres != 0)
    return walkres;
  
  for(int i=0; i<MAX_PAGES_ON_DISK; i++)
  {
    struct pageondisk* page = &p->pagesondisk[i];
    if(page->va != 0 && page->va == va)
      swap_in_by_va(p, page->va);
      return (pte_t *)page->va;
  }
  return 0;
}

void initCurrPage(struct pageondisk* page)
{
    page->fileoffset = -1;
    page->va = 0;
}

uint64 swap_in_by_va(struct proc* p, uint64 va)
{
	va = PGROUNDDOWN(va);
  uint64 pa = move_random_page_to_disk(p->pagetable, p->pagesondisk);
  struct pageondisk* page = get_diskspace(p, va);
  lazy_read_from_swapfile(p, pa, page->fileoffset, PGSIZE);
  if(!mappages(p->pagetable, va, PGSIZE, pa, page->flags))
    panic("swap_by_va");
  
  pte_t* pte = (pte_t*) va;



  *pte &= ~PTE_PG;
  *pte |= PTE_V;

  

  return 0;
}

int	
lazy_read_from_swapfile(struct proc * p, char* buffer, uint placeOnFile, uint size)
{
  if(p->swapFile == 0)
    createSwapFile(p);
  return readFromSwapFile(p, buffer, placeOnFile, size);
}
int 
lazy_write_to_swapfile(struct proc* p, char* buffer, uint placeOnFile, uint size)
{
  if(p->swapFile == 0)
    createSwapFile(p);
  return writeToSwapFile(p, buffer, placeOnFile, size);
}

int	
lazy_remove_swapfile(struct proc* p)
{
  if(p->swapFile == 0)
    return 0;
  int res = removeSwapFile(p);
  p->swapFile = 0;
  return res;
}


int deep_copy_pages(struct proc *p, struct proc *np)
{
  int vir_counter = 0;
  char* buffer = kalloc();
  for (int i = 0; i < MAX_PAGES_ON_DISK; i++)
  {
    struct pageondisk* page = &p->pagesondisk[i];
    if(page->va == 0)
      continue;
    struct pageondisk* npage = &np->pagesondisk[i];
    *page = *npage;

    int offset = page->fileoffset;
    

    if(readFromSwapFile(p, buffer, offset, PGSIZE) < 0) return -1;
    if(writeToSwapFile(np, buffer, offset, PGSIZE) < 0) return -1;
      
    
  }
  kfree(buffer);
  return 0;
}

int fork_memory(struct proc *np)
{
  struct proc *p = myproc();
  // if (np->swapFile == 0 && p->swapFile != 0)
  // if ((np->swapFile == 0 && np->pid > 2) || (p->pid > 2 && p->swapFile != 0))
  //   if(createSwapFile(np) < 0) return -1;

  return deep_copy_pages(p, np);
}

uint64 vaof(struct proc* p, pte_t* pte)
{
  return (uint64)pte;
}

// void removePages(struct proc* p, pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
// {
//   uint64 a;
//   for (a = va; a < va + npages * PGSIZE; a += PGSIZE){
//     int deleted = 0;


//     for(struct pageondisk* page = &p->pagesondisk[MAX_PAGES_ON_DISK - 1]; page > p->pagesondisk && !deleted; page--) {
//       if(page->pagelocation == PHYSICAL && page->address.memaddress == a && pagetable == p->pagetable && do_free){
//         initCurrPage(page);
//         deleted = 1;
//       }
//       else if(page->pagelocation == VIRTUAL && page->address.fileoffset == a && pagetable == p->pagetable){
//         initCurrPage(page);
//         deleted = 1;
//       }
    
//       if(number_of_used_pages(p) == 0){
//         lazy_remove_swapfile(p);
//         return;
//       }
//     }
//   }
// }


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, struct pageondisk* diskpages, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  struct proc* p = myproc();

  if(newsz < oldsz) {
    return oldsz;
  }
  oldsz = PGROUNDUP(oldsz);

  int newpgscounter = 0;
  for(a = oldsz; a < newsz; a += PGSIZE){
    uint64 newpgsize = newpgscounter < (newsz - oldsz) / PGSIZE ? PGSIZE : newsz % PGSIZE;

    mem = (char*) add_page(p->pagesondisk, pagetable, newpgsize, a, xperm, oldsz);
    if(mem == 0)
      return 0;
    newpgscounter++;
  }
  return newsz;
}