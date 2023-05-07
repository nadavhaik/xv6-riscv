#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define debug_acquire(lock) \
  printf("acquiring lock for %s\n", (lock)->name);  \
  acquire((lock));

#define debug_release(lock) \
  printf("releasing lock for %s\n", (lock)->name);  \
  release((lock));

extern struct proc proc[NPROC];

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
kforkret(void)
{
  // printf("kforkret called!\n");
  static int first = 1;


  // Still holding kt->lock from scheduler.
  release(&mykthread()->lock);


  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

void
freekt(struct kthread *kt)
{
  // if(kt->trapframe)
  //   kfree((void*)kt->trapframe);
  kt->trapframe = 0;
  kt->tid = 0;
  kt->proc = 0;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->state = UNUSED;
  memset(&kt->ctx, 0, sizeof(kt->ctx));
}

void kthreadinit(struct proc *p)
{

  initlock(&p->tlock, "threads allock");
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    // printf("initializing thread at %p\n", kt);
    initlock(&kt->lock, "kt lock");

    kt->state = UNUSED;
    kt->proc = proc;
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  } 

}

struct kthread *mykthread()
{
  push_off();
  struct kthread *kt;
  kt = mycpu()->thread;
  pop_off();
  return kt;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int alloctid(struct proc* p)
{
  int next_tid;
  acquire(&p->tlock);
  next_tid = p->next_tid++;
  release(&p->tlock);
  return next_tid;
}

struct kthread*
allockt(struct proc* proc) 
{
  // printf("allockt called!\n");
  struct kthread *kt;

  for(kt = proc->kthread; kt < &proc->kthread[NKT]; kt++) {
    acquire(&kt->lock);
    if(kt->state == UNUSED) {
      goto found;
    } else {
      release(&kt->lock);
    }
  }
  return 0;

found:
  kt->tid = alloctid(proc);
  kt->state = USED;
  kt->killed = 0;
  kt->trapframe = get_kthread_trapframe(proc, kt);
  kt->proc = proc;

  // // Allocate a trapframe page.
  // if(get_kthread_trapframe(proc, kt) == 0){
  //   freekt(kt);
  //   release(&kt->lock);
  //   return 0;
  // }


  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->ctx, 0, sizeof(kt->ctx));
  kt->ctx.ra = (uint64)kforkret;
  kt->ctx.sp = kt->kstack + PGSIZE;


  return kt;
}

int ktkilled(struct kthread* kt)
{
  acquire(&kt->lock);
  int killed = kt->killed;
  release(&kt->lock);
  return killed;
}
