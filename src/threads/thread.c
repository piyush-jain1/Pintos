#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"

// Changes @ T01

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

// Changes @ T01
static struct list wait_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

// Changes @ T03
const int f = 1<<14;  
int load_avg = 0;
//T03

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */

// Changes @ T03
static thread_func managerial_func;
static struct thread* managerial_thread;
static thread_func calculator_func;
static struct thread* calculator_thread;
//t03

void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  // Changes @ T01
  list_init(&wait_list);

  

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  
}

// Changes @ T03
static void managerial_func (void *param){
  managerial_thread = thread_current();
  while (true)
  {
    enum intr_level old_level;
    old_level = intr_disable();
    //intr_disable();
    thread_block();
    //intr_enable();
    intr_set_level(old_level);
  
    timer_wakeup();
   
  }
} 
//t03


// Changes @ T03
static void calculator_func(void * param){
  calculator_thread = thread_current();

  while (true)
  {
    enum intr_level old_level;
    old_level = intr_disable();
    thread_block();
    intr_set_level(old_level);
    
    //recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice
    //load_avg = (59/60)*load_avg + (1/60)*ready_threads,
    //priority = PRI_MAX - (recent_cpu / 4) - (nice * 2),

    struct list_elem* e;

    int ready_threads = 0;

    for (e = list_begin (&ready_list); e != list_end (&ready_list); e = list_next (e)){
      struct thread * tmp = list_entry(e,struct thread,elem);
      if(tmp == managerial_thread || tmp == idle_thread || tmp == calculator_thread) continue;
      ready_threads++;
    }

    int num = 59*f;
    int den = 60*f;
    int ratio = ((int64_t) num) * f / den ;
    load_avg = ((int64_t)load_avg) * ratio / f;

    int r2 = ((int64_t) f) * f / den ;
    r2 = r2*ready_threads;

    load_avg += r2;

    for(e = list_begin(&all_list);e != list_end(&all_list);e = list_next(e)){
      struct thread * tmp = list_entry(e,struct thread,allelem);
      
      if(tmp == managerial_thread || tmp == idle_thread || tmp == calculator_thread) continue;
      
      int num = 2*load_avg;
      int den = 2*load_avg+f;
      int ratio = ((int64_t) num) * f / den ;
      tmp->recent_cpu = (((int64_t) tmp->recent_cpu) * ratio )/ f + (tmp->nice)*f;

    }
  }
}
//t03


// Changes @ T03
void timer_wakeup(){
  int64_t curTime = timer_ticks();
  if(!list_empty(&wait_list)){

    while(list_empty(&wait_list) == 0){
      struct list_elem* first = list_front(&wait_list);
      struct thread* cur = list_entry(first,struct thread,elem);
      if(cur->wakeTime > curTime) break;
      list_pop_front(&wait_list);
      thread_unblock(cur);
    }
  }
}

//t03

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  // Changes @ T03
  thread_create ("managerial", PRI_MAX, managerial_func, NULL);
  thread_create ("calculator", PRI_MAX, calculator_func, NULL);
  //t03

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (int64_t timePass) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;


  

  if(!list_empty(&wait_list)){
    struct list_elem* first = list_front(&wait_list);
    struct thread* cur = list_entry(first,struct thread,elem);
    if(cur->wakeTime <= timePass && managerial_thread->status == THREAD_BLOCKED){
      // Changes @ T03
      //list_pop_front(&wait_list);
      //thread_unblock(cur);
      thread_unblock(managerial_thread);
      // T03
    }
  }

  // Changes @ T03
  if (timePass % TIMER_FREQ == 0 && thread_mlfqs && calculator_thread->status == THREAD_BLOCKED){
    thread_unblock(calculator_thread);
  }
  //T03

  // Changes @ T03
  if(thread_mlfqs){
    struct thread* curThread = thread_current();
    if(curThread!=managerial_thread && curThread!=idle_thread && curThread!=calculator_thread){
      curThread->recent_cpu += f;
    }
  }
  //T03

  // Changes @ T03
  if (timePass % TIME_SLICE == 0 && thread_mlfqs){
    struct list_elem* e;
    for(e = list_begin(&all_list);e != list_end(&all_list);e = list_next(e)){
      struct thread * tmp = list_entry(e,struct thread,allelem);
      
      if(tmp == managerial_thread || tmp == idle_thread || tmp == calculator_thread) continue;
      
      int recent_cpu = tmp->recent_cpu;
      int nice = tmp->nice;
      int new_priority =  PRI_MAX*f - (recent_cpu / 4) - (nice*f * 2);
      
      if(new_priority >=0)
        new_priority = (new_priority+f/2)/f;
      else
        new_priority = (new_priority-f/2)/f;

      if(new_priority<=PRI_MIN)
        new_priority = PRI_MIN;
      else if(new_priority>=PRI_MAX)
        new_priority = PRI_MAX;

      tmp->priority = new_priority;
    }

    sort_ready_list();
  }
  // T03

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE){
    intr_yield_on_return ();
  }

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  
  /* Add to run queue. */
  thread_unblock (t);
  // Changes @ T02
  if(t->priority > thread_current()->priority) thread_yield();  

  // T03
  //  Changes @ UP02

#ifdef USERPROG
  sema_init (&t->wait, 0);
  t->ret_status = RET_STATUS_DEFAULT;
  list_init (&t->files);
  list_init (&t->children);
  if (thread_current () != initial_thread)
    list_push_back (&thread_current ()->children, &t->children_elem);
  t->parent = thread_current ();
  t->exited = false;
#endif

  return tid;
}



/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */

void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);
  
  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

void thread_block_till_wakeup(int64_t wakeup_at){
  enum intr_level old_level;
  old_level = intr_disable ();
  thread_current()->wakeTime = wakeup_at;

  // Changes @ T01
  list_insert_ordered(&wait_list,&thread_current()->elem,compareWakeup,NULL);
  
  thread_block();
  intr_set_level(old_level);
}


// Changes @ T01

void thread_set_next_wakeup(){
  enum intr_level old_level;
  old_level = intr_disable ();


  if(!list_empty(&wait_list)){
    struct list_elem* first = list_front(&wait_list);
    struct thread* cur = list_entry(first,struct thread,elem);
    if(cur->wakeTime <= thread_current()->wakeTime){
      list_pop_front(&wait_list);
      thread_unblock(cur);
    }
  }

  intr_set_level (old_level);

}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // Changes @ T01
  t->status = THREAD_READY;
  list_insert_ordered(&ready_list,&t->elem,comparePriority,NULL);       
    
  intr_set_level (old_level); 

}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  /* ==  Changes @ UP02 */
  struct list_elem *l;
  struct thread *t, *cur;
  
  cur = thread_current ();

  for (l = list_begin (&cur->children); l != list_end (&cur->children); l = list_next (l))
    {
      t = list_entry (l, struct thread, children_elem);
      if (t->status == THREAD_BLOCKED && t->exited)
        thread_unblock (t); 
      // dont know y unblocking these threads
      else
        {
          t->parent = NULL;
          list_remove (&t->children_elem);
        }
        
    }
  process_exit ();
  // where it is closing opened files
  ASSERT (list_size (&cur->files) == 0);
  if (cur->parent && cur->parent != initial_thread)
    list_remove (&cur->children_elem);
  /* ==  Changes @ UP02 */
  
#endif
  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it call schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

// Changes @ T02 maybe add interupt
void checkYield(){

  // Changes @ T02
  enum intr_level old_level;
  old_level = intr_disable ();

  if(!list_empty(&ready_list)){
    if( thread_current()->priority < list_entry(list_front(&ready_list),struct thread,elem)->priority ){
     thread_yield();
    }
  }
  intr_set_level (old_level);

}

// Changes @ T02 maybe add interupt
void sort_ready_list(){

  // Changes @ T02
  enum intr_level old_level;
  old_level = intr_disable ();


  if(!list_empty(&ready_list)){
    list_sort(&ready_list,comparePriority,NULL);
  }
  intr_set_level (old_level);
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();

  // Changes @ T03
  if(cur == managerial_thread || cur == calculator_thread) return;
  //t03

  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();

  if (cur != idle_thread){
    // Changes @ T01
    list_insert_ordered(&ready_list,&cur->elem,comparePriority,NULL);
    //list_push_back (&ready_list, &cur->elem);
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
  thread_current ()->prev_priority_lock = new_priority;
  struct list* cur_lock_list = &(thread_current()->lock_list);
  struct list_elem *e;
  
  // Changes @ T02
  for(e = list_begin(cur_lock_list); e!= list_end(cur_lock_list); e=list_next(e)){
    struct lock* cur_lock = list_entry(e,struct lock,elem);
    if(thread_current ()->priority < cur_lock->priority_lock)
      thread_current ()->priority = cur_lock->priority_lock;
  }
  
  enum intr_level old_level;
  old_level = intr_disable ();


  if(!list_empty(&ready_list)){
    struct list_elem* first = list_front(&ready_list);
    if(list_entry(first,struct thread,elem)->priority > new_priority){
      thread_yield();
    }
  }
  intr_set_level (old_level);

}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  // ADDT02
  //Implement donation part.
  int priority= thread_current ()->priority;
  return priority;
}

// Changes @ T01
void thread_priority_temporarily_up(){
  thread_current()->prev_priority = thread_current()->priority;
  thread_current()->priority = 63;

}
void thread_priority_restore(){

  thread_current()->priority= thread_current()->prev_priority ;

  enum intr_level old_level;
  old_level = intr_disable ();
  // Changes @ T02 -- maybe check recursive for locks
  if(!list_empty(&ready_list)){
    struct list_elem* first = list_front(&ready_list);
    struct thread* cur = list_entry(first,struct thread,elem);
    if(cur->priority > thread_current()->priority) thread_yield();  
  }
  intr_set_level (old_level);

}
/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{

  // Changes @ T03
  if(thread_mlfqs == false) return 0;

  thread_current()->nice = nice;
  int recent_cpu = thread_current()->recent_cpu;
  int new_priority =  PRI_MAX*f - (recent_cpu / 4) - (nice * 2);
  if(new_priority >=0)
    new_priority = (new_priority+f/2)/f;
  else
    new_priority = (new_priority-f/2)/f;

  if(new_priority<=PRI_MIN)
    new_priority = PRI_MIN;
  else if(new_priority>=PRI_MAX)
    new_priority = PRI_MAX;

  thread_current()->priority = new_priority;

  checkYield();
  //T03
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  // Changes @ T03
  return thread_current()->nice;
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  // Changes @ T03
  int load_avg_value = 100*load_avg;
  if(load_avg_value>=0)
    load_avg_value+=f/2;
  else
    load_avg_value-=f/2;
  load_avg_value/=f;

  return load_avg_value;
  //T03
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  // Changes @ T03
  int recent_cpu_value = 100*thread_current()->recent_cpu;
  if(recent_cpu_value>=0)
    recent_cpu_value+=f/2;
  else
    recent_cpu_value-=f/2;
  recent_cpu_value/=f;

  return recent_cpu_value;
  //T03
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->prev_priority_lock = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

  // Changes @ T02
  list_init(&(t->lock_list));
  t->seeking = NULL;
  t->seeking_lock=NULL;

  // Changes @ T03
  t->nice = 0;
  t->recent_cpu = 0;
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);


// Changes @ T01
bool comparePriority(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED) {

   if( list_entry(a,struct thread,elem) -> priority > list_entry(b,struct thread,elem)->priority ) return true;
   return false;
}
bool compareWakeup(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED) {

   if( list_entry(a,struct thread,elem) -> wakeTime < list_entry(b,struct thread,elem)->wakeTime ) return true;
   return false;
}

//  Changes @ UP02
struct thread *
get_thread_by_tid (tid_t tid)
{
  struct list_elem *f;
  struct thread *ret;
  
  ret = NULL;
  for (f = list_begin (&all_list); f != list_end (&all_list); f = list_next (f))
    {
      ret = list_entry (f, struct thread, allelem);
      ASSERT (is_thread (ret));
      if (ret->tid == tid)
        return ret;
    }
    
  return NULL;
}
