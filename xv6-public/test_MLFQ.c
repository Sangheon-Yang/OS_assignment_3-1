#include "types.h"
#include "stat.h"
#include "user.h"
/*
int main (int argc, char *argv[]) {
	char *buf = "Hello xv6!";
	int ret_val;
	ret_val = my_syscall(buf);
	printf(1, "Return value : 0x%x\n", ret_val);
	exit();
	printf(1, "My pid is %d\n", getpid());
	printf(1, "My ppid is %d\n", getppid());
	return 0;
}
*/
/*
int main (int argc, char *argv[]) {
	int pid = fork();
	
	while(1) {
		if (pid == 0) {	//child
			printf(1, "child\n");
			yield();
		} else if (pid > 0) {
			printf(1, "parent\n");
			yield();
		} else {
			printf(1, "fork err!\n");
			exit();
		}
		sleep(100);
	}

	return 0;
}
*/
int main (int argc, char *argv[]) {
	int c1, c2, p;
	int pid = fork();

	if (pid == 0) {		// child1
		cpu_share(2);	
		for(c1 = 0; c1 < 1000; c1++)
			printf(1, "child1 : %d\n", c1);
	} else {	
		pid = fork();

		if (pid == 0) {	//child 2
			cpu_share(4);
			for(c2 = 0; c2 < 1000; c2++)
				printf(1, "child2 : %d\n", c2);
		} else {
			run_MLFQ();	//parent
			for(p = 0; p < 1000; p++)
				printf(1, "parent : %d  %d \n", p, getlev());
			wait();
		}
	}
	exit();
/*
	int i;
	printf(1, "result : %d\n\n", run_MLFQ());
	for(i = 0; i < 1000; i ++) 
		printf(1, "%d\n", i);
		//sleep(2);
	exit();
	*/
}
