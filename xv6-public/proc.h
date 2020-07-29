// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

//////edited_for_P2_scheduler////////////////
enum runningIn { MLFQ , STRIDE , CPU_SHARE };
/////////////////////////////////////////////

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
////////edited_for_P2_scheduler////////////////////////////////////////////
  enum runningIn runPlace; // STRIDE,MLFQ,CPUSHARE 중 어느곳에서 돌고있는지 나타냄
  int calledCPUShare;//cpuShare()호출여부; 호출시 1, default = 0
  int ifFixedCPU;//cpuShare() CPU고정비율 할당 여부; 할당될시 1, default = 0
  int calledMLFQ;//run_MLFQ() 호출여부; 호출시 1, default = 0
  unsigned long long pass;//진행정도 default=0;
  int wantedCPUp;//cpuShare()호출시 원하는 고정점유비율; default=0
  int levMLFQ;//MLFQ레벨; 0,1,2; MLFQ호출 안했을시 -1; default = -1
  int tQuantum; // MLFQ한레벨에서의 time quantum //default = -1;
  int tAllotment;// MLFQ한레벨에서의 time allotment 나타냄//default =-1;
  unsigned long long MLFQturn;// MFLQ안에서 실행될 순서를 나타냄.
////////P3_edited///////////////////////////////////////////////////////////
  int isThread; //Thread 인지 아닌지 판별 1일때thread, 0일때Process
  uint initialsz; // sbrk이전의 sz 초기 사이즈.
  struct proc* master; //현 thread를 create한 mainthread를 가리킴.
  int tid; //Thread id를 나타냄. 모든 thread는 unique한tid를 가짐. thread가 아니면 -1;
  void * ret_val;//thread의 return value;
  int hasThread;//process에 속한 thread 의 갯수 나타냄.
  uint stbase; //thread일때stack의 base (혹은 stack의 최대크기), thread만 사용
  int numOfEmptySpace; //종료된 thread의 갯수.//process만 사용
  //(나중에 재사용할 메모리공간을 확보할때 사용)
  uint emptySpace[NPROC];//종료된 thread가 사용하던 스택메모리 영역 trace용도//proc
//  int masterKilledCount; // kill 당한횟수 나타냄
  struct proc *threadZombie; //worker_thread가 exit를 호출했을때 호출한 thread.
////////////////////////////////////////////////////////////////////////////
};
////////edited_for_P2_schduler//////////////////////////////////////////////
extern int tNumOfProc; //전체 프로세스의 숫자 나타냄
extern int numOfStrideProc; //전체프로세스중 일반 Stride 프로세스 숫자 나타냄
extern int numOfCPUShareProc;// CPU고정점유 하는 프로세스의 숫자 나타냄
extern int cpuShareFix;//CPU고정점유비율의 총합 나타냄
extern int numOfMLFQProc;//MLFQ 프로세스의 숫자 나타냄
extern int enableMLFQ;//MLFQ사용하면 20, 아니면 0
extern int levMLFQcount[3]; //MLFQ를 이용하는 레벨별 프로세스 숫자
extern unsigned long long minPass;//default stride 프로세스들의 pass중 최소값
extern unsigned long long maxPass;//default stride 프로세스들의 pass중 최대값
extern unsigned long long passOfMLFQ;//MLFQ 전체에 부여되는 Pass값
extern unsigned long long turnOfMLFQProc;//MLFQ 프로세스에게 부여되는 실행순서를 나타냄
///////////////////////////////////////////////////////////////////////////
//extern static void wakeup1(void *chan);
// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
