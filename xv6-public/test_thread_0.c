#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 50

#define NUM_INCREAMENT 100000

int global_count = 0;

void* function(void* arg){
  int local_count=0;
  int i;
  for(i=0 ; i<NUM_INCREAMENT; i++){
    global_count+=(int)arg;
    local_count+=(int)arg;
  }
  thread_exit((void*)local_count);
  return 0;
}

int main(int argc, char** argv){
  
  int i, ret;
  thread_t thr[NUM_THREAD];
  int a = 2;

  for(i = 0; i<NUM_THREAD ; i++){
    if(thread_create(&thr[i], function, (void*)a)<0){
      printf(1, "thread_create_ error!!\n");
      return -1;
    }
  }
 
  for(i = 0; i<NUM_THREAD ; i++){
    thread_join(thr[i],(void**)&ret);
    printf(1, "thread %d local: %d\n",thr[i], ret);
  }

  printf(1, "global: %d \n", global_count);
  
  exit();
}
