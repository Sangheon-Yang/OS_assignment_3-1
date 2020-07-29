#include "types.h"
#include "stat.h"
#include "user.h"

int main(int agrc , char* argv[]){
	int pid;
	pid = fork();
	if (pid == 0){
		run_MLFQ();
		printf(1, "child MLFQ = %d\n", getlev());
	}
	else if(pid>0){
		wait();
		printf(1, "parent cpu share = 10 %d\n", cpu_share(10) );
	}
	return 0;
}
