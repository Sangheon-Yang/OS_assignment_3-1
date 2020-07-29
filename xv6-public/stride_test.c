#include "types.h"
#include "user.h"
#include "stat.h"
int main(int argc, char *argv[])
{
	int c,p;
	int pid = fork();
	c=p=0;
	if(pid==0){
		cpu_share(2);
		for(c=0; c<2000 ; c++){
			printf(1, "child: %d\n", c);
 		}
	}else{
		cpu_share(8);
		for(p=0; p<2000; p++){
			printf(1, "parent: %d\n",p);
		}
	}
	exit();
	//return 0;

}
	
