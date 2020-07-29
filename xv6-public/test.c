#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){
	printf(1, "pid is %d\n", getpid());
	printf(1, "ppid is %d\n", getppid());
	exit();
}
