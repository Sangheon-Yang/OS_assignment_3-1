#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


//////edited_for_P2_scheduler(global variables)///////////
int tNumOfProc = 0; //전체 프로세스의 숫자
int numOfStrideProc = 0; //전체 default Stride 프로세스의 숫자
int numOfCPUShareProc = 0; //cpu 고정점유중인 프로세스의 숫자
int cpuShareFix = 0; //cpu 고정점유비율 총합
int numOfMLFQProc = 0; //MLFQ 프로세스 숫자
int enableMLFQ = 0;//MLFQ를 사용하면 20 아니면 0
int levMLFQcount[3] = {0,0,0};//MLFQ레벨별 프로세스숫자
int boost = 1;//MLFQ 부스팅 떄 사용하는 변수; MLFQ에 사용된 tick 의미
unsigned long long minPass;//프로세스들의 패스중 가장 작은 패스
unsigned long long maxPass;//프로세스들의 패스중 가장 큰 패스
unsigned long long passOfMLFQ = -1;//MLFQ전체의 pass, 초기값은 maximum value -1
int numOfSleepingProc=0;//sleeping proc숫자;pass 계산에 사
unsigned long long tempMLFQturn; //MLFQturn minimum 값 임시로 저장
unsigned long long turnOfMLFQProc = 0;//MLFQ프로세스의 실행순서 나타내는 숫자
struct proc * rp;//선택된 프로세스
//////////////////////////////////////////////////////////
////////P3_edited////////////////////////////////////////////
int nextTid = 1;//thread에게 부여될 thread id
////////////////////////////////////////////////////////////


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  ///P3_edited/////////////////////////
  p->tid = 0;
  p->isThread = 0;
  p->hasThread = 0;
  p->master = 0;
  p->ret_val = 0;
  ////edited_for_P2_sheduler////////////////////////////////////////////
  p->runPlace = STRIDE;//처음엔 default stride로 실행
  p->calledCPUShare = 0;
  p->calledMLFQ = 0;
  p->wantedCPUp = 0;
  p->ifFixedCPU = 0;
  p->levMLFQ = -1;
  p->tQuantum = -1;
  p->tAllotment = -1;
  if(tNumOfProc < 2){//userinit, shell 고려하여 첫 두 프로세스는 pass 0으로 시작
    p->pass = 0;
  }
  else{
    p->pass = maxPass;//프로세스가 생성되면 maxPass 값 할당
  }
  tNumOfProc++; // 전체 프로세스 숫자 1증가
  numOfStrideProc++; // default stride 프로세스 숫자 1증가
  //////////////////////////////////////////////////////////////////

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
/////
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
/*int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}*/


///P3_edited//
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  struct proc *master;
  struct proc *p;
  pde_t *pgdir;  
  acquire(&ptable.lock);

  if(curproc->isThread > 0){
    master = curproc->master;
  }
  else{
    master = curproc;
  }
  sz = master->sz;
  pgdir = master->pgdir;

  if(n > 0){
    if((sz = allocuvm(pgdir, sz, sz + n)) == 0){
	  release(&ptable.lock);
      return -1;
	}
	if(master->hasThread>0){//worker thread들에게 모두 적용
	  for(p=ptable.proc; p<&ptable.proc[NPROC];p++){
	    if(p->pid == master->pid && p->tid > 0 && p->state != ZOMBIE){
		  p->pgdir = pgdir;
		  p->sz = sz;
	    }
	  }
    }
  } 
  else if(n < 0 && master->hasThread == 0){
    if((sz = deallocuvm(master->pgdir, sz, sz + n)) == 0){
      release(&ptable.lock);
	  return -1;
	}
  }
  else{ // master->hasThread > 0 && n<0 is dangerous
    release(&ptable.lock);
	return -1;
  }

  master->sz = sz;
  master->pgdir = pgdir;

  release(&ptable.lock);
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
//
////////P3_edited/////////////////
//modified version of fork////////////

int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  acquire(&ptable.lock);
  //P3_edited//////////////////////
  //thread인 경우.
  if(curproc->isThread > 0){
//	acquire(&ptable.lock);
    if((np->pgdir = copyuvm(curproc->master->pgdir, curproc->master->sz)) == 0){
      kfree(np->kstack);
	  np->kstack = 0;
	  np->pid =0;
	  np->tid=0;
	  np->state = UNUSED;
//	  release(&ptable.lock);
	  return -1;
	}
//	release(&ptable.lock);
  }
  //////////////////////////////////
  else{
    // Copy process state from proc.
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
	  np->pid = 0;
	  np->tid = 0;
      np->state = UNUSED;
      return -1;
    }
  }
//  release(&ptable.lock);

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

//  acquire(&ptable.lock);
  
  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}
//original fork//
/*
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
	np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  
  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}*/


////////////////////P3_edited////////////////////////////////////////////

int
thread_create(thread_t* thread , void*(*start_routine)(void *), void* arg)
{//fork + exec 함수의 조합으로 이뤄짐
  int i;
  uint sz, vstbase, sp;
  struct proc *np;
  struct proc *curproc = myproc();
  pde_t *pgdir;

  //corner case
  if(curproc->isThread == 1){// thread 가 thread 를 생성하는것 deny
    return -1;
  }

  nextpid--;//pid 전역변수 고정
  if((np = allocproc()) == 0){
    return -1;
  }

  acquire(&ptable.lock);
  np->master = curproc; //master 표시
  np->pid = curproc->pid;//master proc과 pid 공유
  np->tid = nextTid++; //unique한 tid 부여
  np->isThread = 1;//thread임을 표시
  np->parent = 0; // 모든 thread는 parent가 없다.
  curproc->hasThread += 1;//master proc의 딸려있는 thread 갯수 1 증가

 // acquire(&ptable.lock);

  pgdir = curproc->pgdir;//master의 pgdir 공유

  if(curproc->numOfEmptySpace > 0){//사용가능한 비어있는 page가 있으면 그 pg 주소에 스택할당
    vstbase = curproc->emptySpace[--curproc->numOfEmptySpace];//빈공간의 갯수 1감소
  }

  else{//빈공간이 없을때 메인스레드의 사이즈 증가, 새로운 페이지 할당.
    vstbase = curproc->sz; //main thread의size증가
    curproc->sz += 2*PGSIZE; // exec함수를 참고하여 2개의 page할당
  }

  ///생성된 thread의 stack 영역 할당. exec함수와 비슷
  if((sz = allocuvm(pgdir, vstbase, vstbase + 2*PGSIZE)) == 0){
    np->state = UNUSED;
    return -1;
  }
//  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
 
  
//  release(&ptable.lock);
 
  *np->tf = *curproc->tf;//trapframe 복사, state를 복사

  for(i = 0; i < NOFILE; i++){
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  }
  np->cwd = idup(curproc->cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name)); 


  sp = sz - 4; // stack pointre
  *((uint*)sp) = (uint)arg;
  sp -= 4;
  *((uint*)sp) = 0xffffffff;//fake return pc
  
  np->pgdir = pgdir; // 
  np->stbase = vstbase;//생성된 스레드의 스택 베이스
  np->initialsz = sz;//생성된 스레드의 사이즈
  np->sz= sz;
  np->tf->eip = (uint)start_routine;//스레드가 시작될 부분 
  np->tf->esp = sp;//새로운 스레드의 스택 포인터 설정

  *thread = np->tid;// tid 값을 thread변수에 저장

//  acquire(&ptable.lock);

  //스케줄러쥴링 policy///////////////////
  if(curproc->runPlace == CPU_SHARE){
    np->calledCPUShare = 1;
    np->wantedCPUp = curproc->wantedCPUp;
    if((cpuShareFix + np->wantedCPUp) <= 20){
      np->ifFixedCPU = 1;
      np->runPlace = CPU_SHARE;
      numOfStrideProc--;
      numOfCPUShareProc++;
    }
  }
  else if(curproc->runPlace == MLFQ){
    np->runPlace = MLFQ;
    np->calledMLFQ = 1; 
    np->levMLFQ = 0;
    np->tQuantum = 1;
    np->tAllotment = 5; 
    numOfMLFQProc++;
    numOfStrideProc--;
    turnOfMLFQProc++;
    np->MLFQturn = turnOfMLFQProc;
    levMLFQcount[0]++;
  }
  ///////////////////////////////

  np->state = RUNNABLE;

  release(&ptable.lock);
  return 0;
}

//////P3_edited////////////////////////////////////////////////////////////////////
// thread interaction modified version
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *master;
  struct proc *p;
  struct proc *p1;
  int fd;

  if(curproc == initproc)
    panic("init exiting");


//----------------------------------------------------------------//
// exit()호출한 대상의 종류에 따라 3가지 경우의 수로 나뉜다.
//  (1) : worker thread가 호출한경우
//  (2) : worker thread를 가지는 master process가 호출하는경우
//  (3) : worker thread를 가지지 않는 일반 process가 호출한경우.
//----------------------------------------------------------------//

//----------------------------------------------------------------//
// 1. worker thread가 exit를 호출하는경우.
//----------------------------------------------------------------//
  if(curproc->isThread == 1){

    //(1). 호출한 worker thread 와 master를 제외한 thread들을 정리해준다.
    acquire(&ptable.lock);
    master = curproc->master;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->isThread == 1 && p->pid == master->pid && p->tid != curproc->tid){
		  
		if(p->state != ZOMBIE){
          release(&ptable.lock);

		  for(fd = 0; fd < NOFILE; fd++){
	        if(p->ofile[fd]){
		      fileclose(p->ofile[fd]);
			  p->ofile[fd] = 0;
		    }
		  }
		  begin_op();
		  iput(p->cwd);
		  end_op();
		  p->cwd = 0;

		  acquire(&ptable.lock);
		
          //Pass abandoned children to initproc
          for(p1 = ptable.proc; p1<&ptable.proc[NPROC]; p1++){
	        if(p1->parent == p){
			  p1->parent = initproc;
			  if(p1->state == ZOMBIE)
			    wakeup1(initproc);
		    }
		  }

		  p->state = ZOMBIE;
		  
		  //scheduling policy/////////////////////
		  ///edited_for_P2_scheduler/////////////////////////////
          if(p->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
            numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
            cpuShareFix = cpuShareFix-(p->wantedCPUp);// 고정비율 조정
          }
          else if(p->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
            numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
            levMLFQcount[(p->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
            if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
              enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
              passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
            }
          }
          else{ //default stride 의 경우 
            numOfStrideProc--;//default stride 프로세스 숫자 1감소
          }
          tNumOfProc--;//전체 프로세스 숫자 1감소
          ////////////////////////////////////////////////////////////////////
		}
		//thread_join에서 thread의 자원을 정리해주는 부분
        master->hasThread -= 1;

		kfree(p->kstack);
		p->kstack = 0;
		p->tid =0;
		p->master = 0;
		p->isThread = 0;
		p->pid =0;
		p->name[0] = 0;
		p->killed = 0;

		p->state = UNUSED;

	    deallocuvm(p->pgdir, p->initialsz, p->stbase);
      }
    }
	release(&ptable.lock);


    //(2). master 를 정리해준다.

    for(fd = 0; fd < NOFILE; fd++){
      if(master->ofile[fd]){
        fileclose(master->ofile[fd]);
        master->ofile[fd] = 0;
      }
    }
    begin_op();
    iput(master->cwd);
    end_op();
    master->cwd = 0;
 
    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent == master){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
    }
	//scheduling policy///
    /////edited_for_P2_scheduler///////////////////////////////////////////
    if(master->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
      numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
      cpuShareFix = cpuShareFix-(master->wantedCPUp);// 고정비율 조정
    }
    else if(master->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
      numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
      levMLFQcount[(master->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
      if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
        enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
        passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
      }
    }
    else{ //default stride 의 경우 
      numOfStrideProc--;//default stride 프로세스 숫자 1감소
    }
    tNumOfProc--;//전체 프로세스 숫자 1감소
    //////////////////////////////////////////////////////////////////////

    master->state = ZOMBIE;

    release(&ptable.lock);

	//(3). 마지막으로 호출한 worker thread를 정리해준다. 
	//	   이때 master->threadZombie로 이 thread를 추가하여 wait()에서 처리.

    for(fd = 0; fd < NOFILE; fd++){
      if(curproc->ofile[fd]){
        fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }    
	}
    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
    }
	//scheduling policy
    /////edited_for_P2_scheduler///////////////////////////////////////////
    if(curproc->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
      numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
      cpuShareFix = cpuShareFix-(curproc->wantedCPUp);// 고정비율 조정
	}
    else if(curproc->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
      numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
      levMLFQcount[(curproc->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
      if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
        enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
        passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
      }
    }
    else{ //default stride 의 경우 
      numOfStrideProc--;//default stride 프로세스 숫자 1감소
    }
    tNumOfProc--;//전체 프로세스 숫자 1감소
    //////////////////////////////////////////////////////////////////////

	master->threadZombie = curproc; // curproc의 자원을 wait()에서 처리

	curproc->state = ZOMBIE;

	wakeup1(master->parent); //master->parent가 master와 curproc을 정리한다.
    sched();
    panic("zombie exit");
  }

  

  //--------------------------------------------------------------//
  // 2. master가 호출하는경우
  //--------------------------------------------------------------//
  else if(curproc->hasThread > 0){

	//(1). 딸려있는 worker thread 들을 정리해준다.
    acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if(p->master == curproc){
		if(p->state != ZOMBIE){
		  release(&ptable.lock);
          for(fd = 0; fd < NOFILE; fd++){
            if(p->ofile[fd]){
              fileclose(p->ofile[fd]);
              p->ofile[fd] = 0;
            }
          }
          begin_op();
          iput(p->cwd);
          end_op();
          p->cwd = 0;
		  acquire(&ptable.lock);

          //pass abandoned children to initproc
          for(p1 = ptable.proc; p1<&ptable.proc[NPROC]; p1++){
	        if(p1->parent == p){
			  p1->parent = initproc;
			  if(p1->state == ZOMBIE)
			    wakeup1(initproc);
		    }
		  }
		  p->state = ZOMBIE;
        
		  //scheduling policy
		  /////edited_for_P2_scheduler///////////////////////////////////////////
          if(p->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
            numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
            cpuShareFix = cpuShareFix-(p->wantedCPUp);// 고정비율 조정
          }
          else if(p->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
            numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
            levMLFQcount[(p->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
            if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
              enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
              passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
            }
          }
          else{ //default stride 의 경우 
            numOfStrideProc--;//default stride 프로세스 숫자 1감소
          }
          tNumOfProc--;//전체 프로세스 숫자 1감소
          //////////////////////////////////////////////////////////////////////
		}
		curproc->hasThread -= 1;
	
        kfree(p->kstack);
        p->kstack = 0;
        p->pid = 0;
		p->tid = 0;
		p->master =0;
		p->isThread = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

		deallocuvm(p->pgdir, p->initialsz, p->stbase);
	  }
	}
    release(&ptable.lock);
  
    // (2). 호출한 master 를 정리
	//  여기서부턴 원래의 exit()와 동일
    for(fd = 0; fd < NOFILE; fd++){
      if(curproc->ofile[fd]){
        fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }
    }
    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;
    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
    }
    /////edited_for_P2_scheduler///////////////////////////////////////////
    if(curproc->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
      numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
      cpuShareFix = cpuShareFix-(curproc->wantedCPUp);// 고정비율 조정
    }
    else if(curproc->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
      numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
      levMLFQcount[(curproc->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
      if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
        enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
        passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
      }
    }
    else{ //default stride 의 경우 
      numOfStrideProc--;//default stride 프로세스 숫자 1감소
    }
    tNumOfProc--;//전체 프로세스 숫자 1감소
    //////////////////////////////////////////////////////////////////////
  // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");
  }
 
  //------------------------------------------------------//
  // 3.thread가 없는 일반 process의 경우
  //-------------------------------------------------------//
  else{
	// 이경우 원래의 exit함수과 같음;
    for(fd = 0; fd < NOFILE; fd++){
      if(curproc->ofile[fd]){
        fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }
    }
    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;
 
    acquire(&ptable.lock);
    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);
    // Pass abandoned children to init.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
    }
    /////edited_for_P2_scheduler///////////////////////////////////////////
    if(curproc->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
      numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
      cpuShareFix = cpuShareFix-(curproc->wantedCPUp);// 고정비율 조정
    }
    else if(curproc->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
      numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
      levMLFQcount[(curproc->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
      if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
        enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
        passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
      }
    }
    else{ //default stride 의 경우 
      numOfStrideProc--;//default stride 프로세스 숫자 1감소
    }
    tNumOfProc--;//전체 프로세스 숫자 1감소
    //////////////////////////////////////////////////////////////////////

  // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");
  }
}

/*
 //원래 exit 함수
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  /////edited_for_P2_scheduler///////////////////////////////////////////
  if(curproc->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
    numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
    cpuShareFix = cpuShareFix-(curproc->wantedCPUp);// 고정비율 조정
  }
  else if(curproc->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
    numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
    levMLFQcount[(curproc->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
    if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
      enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
      passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
    }
  }
  else{ //default stride 의 경우 
    numOfStrideProc--;//default stride 프로세스 숫자 1감소
  }
  tNumOfProc--;//전체 프로세스 숫자 1감소
  //////////////////////////////////////////////////////////////////////

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
*/

////////////////P3_edited////////////////////////////////////

void
thread_exit(void* retval)
{//exit함수 모방하여 만듬
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;
  //corner_case : workerthread만이 thread_exit를 호출할수 있다.
  if(curproc->isThread == 0){
    panic("not thread");
  }
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
///////////////////////////////////////
  acquire(&ptable.lock);
  curproc->state = ZOMBIE;
  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  curproc->ret_val = retval;//return value저장
  wakeup1(curproc->master);
  //for scheduling
  /////edited_for_P2_scheduler///////////////////////////////////////////
  if(curproc->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
    numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
    cpuShareFix = cpuShareFix-(curproc->wantedCPUp);// 고정비율 조정
  }
  else if(curproc->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
    numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
    levMLFQcount[(curproc->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
    if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
      enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
      passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
    }
  }
  else{ //default stride 의 경우 
    numOfStrideProc--;//default stride 프로세스 숫자 1감소
  }
  tNumOfProc--;//전체 프로세스 숫자 1감소
  ////////////////////////////////////////////////////////////////////
  sched();
  panic("zombie thread exit");
}

/////////////////////////////////////////////////////////////////////////


// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  struct proc *tZ;
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){

		if(p->threadZombie->state == ZOMBIE){
		  tZ = p->threadZombie;
		  kfree(tZ->kstack);
		  tZ->kstack =0;
		  tZ->tid =0;
		  tZ->master = 0;
		  tZ->isThread = 0;
		  tZ->pid = 0;
		  tZ->name[0] =0;
		  tZ->killed = 0;
		  tZ->state = UNUSED;
		  deallocuvm(tZ->pgdir , tZ->initialsz, tZ->stbase);
		}

        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
//////////////P3_edited////////////////////////////////

int
thread_join(thread_t thread , void **retval)
{//wait함수 모방하여 만듬
  struct proc *p;
  struct proc *curproc = myproc();

  if(curproc->isThread == 1){//mainthread(proc)가 아니면 join을 호출할수 없음
    return -1;
  }

  if(curproc->hasThread == 0){//thread가 없는경우
	return -1;
  }

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->tid != thread)
        continue;

      if(p->master != curproc){
        release(&ptable.lock);
        return -1;
      }

      if(p->state == ZOMBIE){
        // Found one.
        *retval = p->ret_val;//ret_val 전달
		curproc->emptySpace[curproc->numOfEmptySpace] = p->stbase;//빈공간 추가
		curproc->numOfEmptySpace++;
        kfree(p->kstack);
        p->kstack = 0;
//		freevm(p->pgdir);
		p->tid = 0;
		p->master =0;
		p->isThread = 0;
        p->pid = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
  	    //thread가 사용하던 stack영역 메모리 정리
		deallocuvm(p->pgdir, p->initialsz, p->stbase);
		
		curproc->hasThread -= 1;

        release(&ptable.lock);

        return 0;
      }
    }
    // No point waiting if we don't have any children.
    if(curproc->killed){
      release(&ptable.lock);
      return -1;
    }
    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

///////////////////////////////////////////////////////////


//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.


/////edited_for_P2_scheduler////////////////////////////////////////////////
void
scheduler(void)
{  
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.용
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
 
    //boosting MLFQ///
    if(numOfMLFQProc>0 && boost == 100){
        levMLFQcount[0] = levMLFQcount[0] + levMLFQcount[1] + levMLFQcount[2];
        levMLFQcount[1] = 0;
        levMLFQcount[2] = 0;
        for(p=ptable.proc; p < &ptable.proc[NPROC] ; p++){
            if(p->state == RUNNABLE && p->runPlace == MLFQ){
                p->levMLFQ = 0;
                p->tQuantum = 1;
                p->tAllotment = 5;
            }
        } 
        boost = 1;
    }
 
    minPass = -1;
    maxPass = 0;
    tempMLFQturn = turnOfMLFQProc;
    numOfSleepingProc = 0;

    //pass가 가장 작은 proc을 선택(MLFQ제외), sleeping proc 의 숫자 계산
    for(p = ptable.proc ; p < &ptable.proc[NPROC] ; p++){
      if(p->state == RUNNABLE && p->runPlace != MLFQ){
        if(p->pass < minPass){
          minPass = p->pass;
          rp = p;
        }
        if(p->pass > maxPass){ 
          maxPass = p->pass;
        }
      }
      if(p->state == SLEEPING && p->runPlace != MLFQ){
        numOfSleepingProc++;
      }
    }
	if(tNumOfProc == numOfSleepingProc){
	  wakeup1(initproc);
	  rp = initproc;
	}

    //MLFQ랑 stride Pass 비교후 패스가 작은것 실행
    if(numOfMLFQProc > 0 && passOfMLFQ < minPass){

      //실핼시텨야할 MLFQproc 선택////////////////////////////////////////////////
      if(levMLFQcount[0] > 0){//level0프로세스가 존재하는경우 
        for(p=ptable.proc ; p< &ptable.proc[NPROC] ; p++){
          if(p->state == RUNNABLE && p->levMLFQ == 0 && p->MLFQturn <= tempMLFQturn){
            rp = p;
            tempMLFQturn = p->MLFQturn;
          }
        }
      } 
      else if(levMLFQcount[0]==0 && levMLFQcount[1] > 0){//level0없고level1에 존재하는경우
        for(p=ptable.proc ; p< &ptable.proc[NPROC] ; p++){
          if(p->state == RUNNABLE && p->levMLFQ == 1 && p->MLFQturn <= tempMLFQturn){
            rp = p;
            tempMLFQturn = p->MLFQturn;
          }
        }
      }
      else{//level2 에만 있을때
        for(p=ptable.proc ; p< &ptable.proc[NPROC] ; p++){
          if(p->state == RUNNABLE && p->levMLFQ == 2 && p->MLFQturn <= tempMLFQturn){
            rp = p;
            tempMLFQturn = p->MLFQturn;
          }
        }
      }
      ///////////////////////////////////////////////////////////////////////////

      
      //실행시킬 MLFQ proc 선택후 정보 업데이트//////////////////////////////////////
      if(rp->levMLFQ == 2) {//level2에 있던 경우
        if(rp->tQuantum == 1){//time quantum을 다 사용한경우
          rp->tQuantum = 4;
          turnOfMLFQProc++;
          rp->MLFQturn = turnOfMLFQProc;
        }
        else{//time quantum 아직 다 안사용한 경우
          rp->tQuantum--;
        }
      } 
      else if(rp->levMLFQ == 1) {//level1 일때
        if(rp->tQuantum == 1 && rp->tAllotment == 1){//tquantum, allotment 다 사용한경우
          rp->levMLFQ++;//레벨2로  강등
          levMLFQcount[1]--;
          levMLFQcount[2]++;
          turnOfMLFQProc++;
          rp->MLFQturn = turnOfMLFQProc;
          rp->tQuantum = 4;
          rp->tAllotment = 100;
        }
        else if(rp->tQuantum == 1 && rp->tAllotment > 1){//quantum 만 다 사용한경우
          rp->tQuantum = 2;//quantum 업데이트
          rp->tAllotment--;
          turnOfMLFQProc++;
          rp->MLFQturn = turnOfMLFQProc;//순서 다시 부여
        }
        else{//tQuantum == 2 
          rp->tQuantum--;
          rp->tAllotment--;
        }
      }
      else{//leve0 일때
        if(rp->tAllotment == 1){//tAllotmet 다 써서 level 바꿔줘야할때
          rp->levMLFQ++;//레벨 1로 강등
          levMLFQcount[0]--;
          levMLFQcount[1]++;
          turnOfMLFQProc++;
          rp->MLFQturn = turnOfMLFQProc;
          rp->tQuantum = 2;
          rp->tAllotment = 10;
        }
        else{//quantum 만 다 사용한경우
          rp->tAllotment--;
          turnOfMLFQProc++;
          rp->MLFQturn = turnOfMLFQProc;//순서 재배치
        }
      }
      ///////////////////////////////////////////////////////////////////////////////
      
      passOfMLFQ += (1000/20);//MLFQ의 pass 증가
      boost++; //priority부스팅 카운트 1증가
    }

    else if(rp->runPlace == CPU_SHARE){//CPUSHARE proc 이 실행된경우.
      rp->pass += (1000/rp->wantedCPUp);//pass 증가.
    }

    else{//stride의경우
      if(numOfStrideProc > 2){
        rp->pass += (((numOfStrideProc-numOfSleepingProc)*1000)/(100-enableMLFQ-cpuShareFix));
      }
      else{//userinit, shell 고려
        rp->pass = (numOfStrideProc*1000)/(100-enableMLFQ-cpuShareFix);
      }
    }
     
    if(numOfMLFQProc == 0){//MLFQproc 이 하나도 없어지면 boosting tick 초기화
      boost=1;
    }
/////////////////////////////////////////////////////////////////////////////////////

    c->proc = rp;
    switchuvm(rp);
    rp->state = RUNNING;
    
    swtch(&(c->scheduler),rp->context);
    switchkvm();
    c->proc = 0;

    release(&ptable.lock);
  }
}

//원래 스케줄러////////////////////////////////////////////
/*
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
*/


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      //edited_for_P2_scheduler///
      if(p->runPlace != MLFQ){
        p->pass = maxPass;
      }
      ///////////////////////////
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).i

int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != ZOMBIE){
	  ///P3_edited/
      p->killed = 1;
        // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}


/*
int
kill(int pid)
{
  struct proc *p;
//  struct proc *tmp;
//  struct proc *master;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){

	  if(p->isThread > 0){//thread 인경우
		p->master->killed = 1;

		if(p->master->state == SLEEPING)
		  p->master->state = RUNNABLE;
		release(&ptable.lock);
		return 0;

	  }
      else{// process의 경우
        p->killed = 1;
        // Wake process from sleep if necessary.
        if(p->state == SLEEPING)
          p->state = RUNNABLE;
        release(&ptable.lock);
        return 0;
	  }
    }
  }
  release(&ptable.lock);
  return -1;
}
*/
/*
threadKilledOnce:
  if(tmp->isThread > 0){
	master = tmp->master;
	master->threadZombie = tmp;
  }
  else{
	master = tmp;
  }

  if(master->masterKilledCount == 1){
    master->killed =1;
	master->masterKilledCount = 0;
	if(master->state == SLEEPING){
	  master->state = RUNNABLE;
	}
  }
  else{
    master->masterKilledCount = 1;
  }
  release(&ptable.lock);
  return 0;

}*/


//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


void execClean(struct proc* caller){
  int fd;
  struct proc * p;
  struct proc * p1;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->isThread == 1 && p->pid == caller->pid){

		if(p->state != ZOMBIE){

          release(&ptable.lock);
		  for(fd = 0; fd < NOFILE; fd++){
	        if(p->ofile[fd]){
		      fileclose(p->ofile[fd]);
			  p->ofile[fd] = 0;
		    }
	 	  }
		  begin_op();
		  iput(p->cwd);
		  end_op();
		  p->cwd = 0;

		  acquire(&ptable.lock);
		
          //Pass abandoned children to initproc
          for(p1 = ptable.proc; p1<&ptable.proc[NPROC]; p1++){
	        if(p1->parent == p){
			  p1->parent = initproc;
			  if(p1->state == ZOMBIE)
			    wakeup1(initproc);
		    }
		  }
		  p->state = ZOMBIE;
		  //scheduling 조정
		  ///edited_for_P2_scheduler///////////////////////////////////////////
          if(p->runPlace == CPU_SHARE){//CPUShare 로 고정할당 된 프로세스였을 경우
            numOfCPUShareProc--; //고정점유 프로세스 숫자 1 감소.
            cpuShareFix = cpuShareFix-(p->wantedCPUp);// 고정비율 조정
          }
          else if(p->runPlace == MLFQ){//MLFQ 프로세스 였을 경우.
            numOfMLFQProc--; //MLFQ프로세스 숫자 1 감소
            levMLFQcount[(p->levMLFQ)]--;//프로세스가 속한 레벨안의 프로세스 숫자 1감소.
            if(numOfMLFQProc == 0){//MLFQ프로세스가 0개가 되었을때
              enableMLFQ = 0; //20%할당을 0%으로 바꿔줌
              passOfMLFQ = -1; // MLFQ pass 값을 최대큰수로 바꿈
            }
          }
          else{ //default stride 의 경우 
            numOfStrideProc--;//default stride 프로세스 숫자 1감소
          }
          tNumOfProc--;//전체 프로세스 숫자 1감소
          ////////////////////////////////////////////////////////////////////

		}
        caller->hasThread -= 1;
		kfree(p->kstack);
		p->kstack = 0;
		//p->tid =0;
		p->isThread = 0;
		p->master=0;
		p->pid =0;
		p->name[0] = 0;
		p->killed = 0;
		p->state = UNUSED;
		if(p->tid != 0){
	      deallocuvm(p->pgdir, p->initialsz, p->stbase);
        }
		p->tid = 0;
      }
  }
  release(&ptable.lock);
}
