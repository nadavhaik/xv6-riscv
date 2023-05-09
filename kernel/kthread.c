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

int kthread_create( void *(*start_func)(), void *stack, uint stack_size ){

  struct kthread* kt;
  int ktid = -1;

  if ((kt = allockt(myproc())) == 0){
    return -1;
  }

  ktid = kt->tid;
  kt->trapframe->epc = (uint64)start_func;
  kt->trapframe->sp = (uint64)(stack + stack_size);

  kt->state = RUNNABLE;
  release(&kt->lock);


  return ktid; 
}

int kthread_id(){
  
  int k = -1;
  struct kthread* kt = mykthread();
  struct spinlock* lock = &kt->lock;
  
  acquire(lock);
  k = kt->tid;
  release(lock);
  
  return k;
}
  


int kthread_kill(int ktid){
  // printdebug("kthread_kill()\n");
  struct proc* p = myproc();
  struct kthread* kt;

  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);

    if(kt->tid == ktid){
      kt->killed = 1;

      if(kt->state == SLEEPING){
        kt->state = RUNNABLE;
      }
      release(&kt->lock);
      return 0;
    }

    release(&kt->lock);
  }

  return -1;
}


void kthread_wakeup(void *chan)
{
  // printdebug("wakeup(void *chan)\n");

  struct proc *p = myproc();

  for (struct kthread* kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    acquire(&kt->lock);
    
    if(kt->state == SLEEPING && kt->chan == chan) {
      kt->state = RUNNABLE;
    }
    release(&kt->lock);
  }

}


void kthread_exit(int status){
  // printdebug("kthread_exit()\n");
  // struct proc* p = myproc();
  struct kthread* kt = mykthread();


  int everyone_are_dead = 1;

  for (struct kthread* i = kt->proc->kthread; i < &kt->proc->kthread[NKT] && everyone_are_dead; i++)
  {
    acquire(&i->lock);
    if (i->killed == 0)
      everyone_are_dead = 0;
    release(&i->lock);
  }

  if (everyone_are_dead){
    exit(status);
  }

  // if anyone waiting for me to die
  kthread_wakeup(kt);

  acquire(&kt->lock);
  kt->state = ZOMBIE;
  kt->xstate = status;
  sched();
  panic("panic : zombie kthread exit\n");

}

// makes mykthread sleep on channel join_to, to wait for it to finish
void kthread_sleep(struct kthread* mykthread,struct kthread* join_to){
  acquire(&mykthread->lock);


  mykthread->chan = join_to;
  // printdebug("kthread sleep before sched() ktid %d\n", mykthread->ktid);
  // printdebug("kthread jointo id %d state = %s\n",join_to->ktid , ktStateToString(join_to->state));
  // printdebug("kthread mykthread id %d state = %s\n",mykthread->ktid , ktStateToString(mykthread->state));                                           
  mykthread->state = SLEEPING;
  release(&join_to->lock);
  sched();
  // printdebug("kthread sleep after sched()\n");
  
  mykthread->chan = 0;

  release(&mykthread->lock);  
  acquire(&join_to->lock);
}

int kthread_join(int ktid, uint64 status){
  // printdebug("kthread_join().\n");
  // struct kthread *mythread = mykthread();
  struct kthread *kt;
  struct kthread *join_to;
  struct proc *p = myproc();
  int found = 0;

  if (kthread_id() == ktid){
    // printdebug("to join ktid is my ktid \n");
    return -1;
  }

  // find the index of the thread to join to 
  for (kt = p->kthread ; kt < &p->kthread[NKT] && !found; kt++){
    int holdingkth = 1;
    if(!holding(&kt->lock)){
      acquire(&kt->lock);
      holdingkth = 0;
    }
    if(kt->tid == ktid){
      join_to = kt;
      found = 1;
      // printdebug("kthread_join(). calling thread ktid: %d state - %s, jointo ktid - %d state - %s\n", kthread_id(), ktStateToString(mykthread()->state), ktid, ktStateToString(kt->state));
    }
    else if (!holdingkth)
      release(&kt->lock);
  }

  // if not found retuen error
  if(!found){
    // printdebug("thread to join not found \n");
    return -1;
  }

  for(;;){
    
    if(join_to->state == ZOMBIE){
      if(status != 0 &&  copyout(p->pagetable, status,(char *)&join_to->xstate, sizeof(join_to->xstate)) < 0){
        release(&join_to->lock);
        return -1;
      }
      
      freekt(join_to);
      release(&join_to->lock);
      return 0;
    }
  
    kthread_sleep(mykthread(), join_to);  //DOC: wait-sleep
    
  }
}