#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];
struct cpu cpus[NCPU];

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
kforkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
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

static void
freekt(struct kthread *kt)
{
  if(kt->trapframe)
    kfree((void*)kt->trapframe);
  kt->trapframe = 0;
  kt->tid = 0;
  kt->proc = 0;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->state = UNUSED;
}

void kthreadinit(struct proc *p)
{
  acquire(&p->lock);

  initlock(&p->tlock, "threads allock");
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));

    initlock(&kt->lock, "kt lock");
    kt->state = UNUSED;
    kt->proc = proc;
  } 

  release(&p->lock);
}

struct kthread *mykthread()
{
  return mycpu()->thread;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int alloctid(struct proc* p)
{
  acquire(&p->tlock);
  int next_tid = p->next_tid++;
  release(&p->tlock);
  return next_tid;
}

struct kthread*
allockt(struct proc* proc) 
{
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

  // Allocate a trapframe page.
  if(get_kthread_trapframe(proc, kt) == 0){
    freekt(kt);
    release(&kt->lock);
    return 0;
  }


  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->ctx, 0, sizeof(kt->ctx));
  kt->ctx.ra = (uint64)kforkret;
  kt->ctx.sp = kt->kstack + PGSIZE;


  // TODO: delte this after you are done with task 2.2
  return kt;
}
