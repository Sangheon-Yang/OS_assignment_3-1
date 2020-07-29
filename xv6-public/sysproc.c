#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_getppid(void)
{
  return myproc()->parent->pid;
}


//edited_for_P2_scheduler//////////////////////////////////////////
int
sys_yield(void)
{
  yield();
  return 0;
}

int
sys_run_MLFQ(void)
{//MLFQ system call
  struct proc* p = myproc();
  if(p->state == UNUSED){//
    return -1;
  }
  enableMLFQ = 20;//프로세스들의 pass 계산시 필요한 부분
  p->calledMLFQ = 1;//MLFQcall 표시
  if(p->runPlace == STRIDE){//default stride에서 실행중이었을 경우
    numOfStrideProc--;//stride 프로세스갯수 1 감소.
  }
  else if(p->runPlace == CPU_SHARE){//cpushare로 실행중이였을 경우
    numOfCPUShareProc--;//cpushare 프로세스 갯수 감소
    cpuShareFix = cpuShareFix-(p->wantedCPUp);//cpushare점유비율 총합 조정
  }
  p->runPlace = MLFQ;//runPlace를 MLFQ로 바꿔줌.
  p->levMLFQ = 0;//level0 q에 넣어줌
  numOfMLFQProc++;//mFLQ 프로세스 숫자 1 증가
  levMLFQcount[0]++;//mlFQ level0 프로세스 1증가
  turnOfMLFQProc++;//MLFQ 안에서의 실행순서를 나타냄.
  p->tQuantum = 1;//lev0기준으로 맞춰줌
  p->tAllotment = 5;//lev0기준으로 맞춰줌
  p->MLFQturn = turnOfMLFQProc;//업데이트된 실행순서 할당.
  if(numOfMLFQProc == 1){//MLFQ가 처음 불린경우 
    passOfMLFQ = maxPass;//MLFQ전체에 maxPass를 부여함.
  }
  return 0;
}

int
sys_getlev(void)
{
  return myproc()->levMLFQ;//프로세스의 레벨 리턴
}

int
sys_cpu_share(int percent)
{ // cpu_share system call
  struct proc *p = myproc();
  argint(0, &percent);//percent 인자 사용위해 필요

  p->calledCPUShare = 1;//호출한것 표시
  p->wantedCPUp = percent;//원하는 비율 저장

  if(percent>0 && percent <= (20-cpuShareFix)){//자리가 남아서 실행되는경우
    p->ifFixedCPU = 1;// 고정점유여부 true
    if(p->runPlace == STRIDE){//default STRIde에서 실행중이였을경우
      numOfStrideProc--; //stride 프로세스 숫자를 1 빼줌.
    }
    else if(p->runPlace == MLFQ){//MLFQ에서 실행중이였을 경우
      numOfMLFQProc--; //MLFQ 프로세스 숫자를 1빼줌.
      levMLFQcount[(p->levMLFQ)]--;//프로세스가 속해있던 레벨에서의 프로세스숫자 조정
    }
    p->runPlace = CPU_SHARE; //runPlace를CPU_SHARE로 바꿔줌.
    numOfCPUShareProc++; //Cpu share 사용하는 프로세스 숫자 1증가
    cpuShareFix = cpuShareFix + percent;//고정점유비율 총합 증가
    p->pass = maxPass;//maxPass 값 부여
    return 0;
  }
  else{//자리가 없어서 실행이 안대는 경우 원래방식대로 실행
    return -1;
  }
}
//////////////////////////////////////////////////////////////////////

////////P3_edited///////////////////////////////////////////////////////
int
sys_thread_create(void)
{
  thread_t* thread;
  void* (*start_routine)(void*);
  void* arg;

  if(argptr(0,(void*)&thread, sizeof(*thread))<0 || argptr(1, (void*)&start_routine, sizeof(*start_routine))<0 || argptr(2, (void*)&arg, sizeof(*arg))<0)
    return -1;
  return thread_create(thread, start_routine, arg);
}

int
sys_thread_exit(void)
{
  int retval;
  if(argint(0, &retval)<0){
    return -1;
  }
  thread_exit((void*)retval);
  return 0;
}

int
sys_thread_join(void)
{ 
  thread_t thread;
  void **retval;
  if(argint(0, &thread)<0 || argptr(1, (void*)&retval, sizeof(**retval))<0)
    return -1;
  return thread_join(thread, retval);
}

/////////////////////////////////////////////////////////////////////////

int
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc* curproc = myproc();

  if(argint(0, &n) < 0)
    return -1;
//  addr = myproc()->sz;

  if(curproc->isThread > 0)
    addr = curproc->master->sz;
  else
    addr = curproc->sz;

  if(growproc(n) < 0)
    return -1;

  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
//////////P4_edited////////////////////////
int
sys_sync(void)
{
  sync();
  return 0;
}

int sys_get_log_num(void)
{
  return get_log_num();
}
////////////////////////////////////////////

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
