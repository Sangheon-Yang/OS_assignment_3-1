#include "types.h"
#include "stat.h"
#include "user.h"

int main (int argc , char* argv[]){
	int pid = -1;
	int i =0;
	pid = fork();
	for(i=0;i<200;i++){
		if(pid == 0){
			printf(1, "Child\n");
			yield();
		}
		else if(pid > 0){
			printf(1, "Parent\n");
			yield();
		}
		else{
			printf(1,"fork error\n");
		}
	}
	wait();
	exit();
	return 0;
}
