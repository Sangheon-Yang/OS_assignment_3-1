#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wait.h>

int main( int argc , char * argv[] ) {
    
    pid_t pid;//자식프로세스를 생성하기위한 process id 변수,
    //현재프로세스의 번호를 저장
    
    char commandline[1024]; //사용자가 입력한 커맨드를 개행단위로 저장하는 문자열,
    //배치파일 사용시 파일에서 한줄찍 읽어와 저장하는 문자열,//최대길이는 100자
    //ex): [prompt> ls ; ls -alf ; pwd\n] =>
    //[commandline = "ls ; ls -alf ; pwd\n"]
    
    char * new_argv[512];
    //사용자가 입력한 명령어를 ";" 과 "\n" 로 구분하여 명령어 조각들을 차곡차곡 저장
    //ex): [prompt> ls ; ls -al\n] =>
    //new_argv[0]="ls ", new_argv[1]=" ls -al"
    
    char * execparam[512]; //exevpc() 때 필요한 인자.
    //new_argv[]에 저장된 스트링을 다시 " " 로 구분하여 차곡차곡 저장
    //ex): new_argv[i]=" ls -al"  ==>> execparam[0]="ls",execparam[1]="-al"
    
    int i = 0;//사용자의 커맨드 한줄을 입력받아 ";" ,"\n" 로 나누는 while문 안에서 사용.
    //커맨드라인 한줄에서 실행해야할 명령어의_갯수 를 의미함. //new_argv[i] 로 사용.
    //ex):prompt> ls ; pwd ; ls -al 일때 명령어의 갯수는 3개임. // i = 0,1,2 가됨.
    
    int j = 0;//i의값이 정해졌을때 i(한 줄 안에 있는 명령어 갯수) 만큼 for문을 돌릴때 사용.
    
    int k = 0;//i의 값만큼 돌아가는 for 문 안에서
    //new_argv[i] 안의 스트링을 execparam[k]로 옮길때 사용.
    //ex):new_argv[0] = " ls -alf"  ==>>
    //execparam[0] = "ls" , execparam[1] = "-alf"
    
    char * tempTokP1; //strtok 함수를 이용하여
    //";"과 "\n"을 기준으로 쪼갠 commandline의 일부분 첫지점을 임시로 저장하는 포인터
    
    char * tempTokP2; //strtok 함수를 이용하여
    //" "을 기준으로 쪼갠 new_argv[i]의 일부분 첫지점을 임시로 저장하는 문자열
    //첫번째 ";","\n" strtok와 , 두번재 " " strtok 를
    //구분하기 위하여 다른 포인터 변수선언(혼돈방지)
    
    FILE * fp = NULL; // 배치파일을 읽어오기위한 파일포인터
    
    if (argc < 2) {// 1. 배치파일 없이 [./shell] 로 실행된 경우 ; (argc==1);
        while (1) {//사용자가 종료하기전까진 계속 입력을 받기위해 무한반복문 실행
            printf("prompt> ");
            if (fgets(commandline, 1024, stdin) == NULL) {
                //사용자가 Ctrl + D 입력시
                printf("\nShell is terminated\n");
                return 0; //shell을 종료함.
            }
            if (strcmp(commandline, "quit\n") == 0) {
                //사용자가 "quit\n" 을 입력할시에도
                printf("Shell is terminated\n");
                return 0;//shell을 종료함
            }
            
            i = 0;//이전 입력때 변경된 i 를 0으로 다시 초기화
            //i는 커맨드 한줄에서 ";"로 나눠진 실행시켜야할 명령어 갯수를 의미
            tempTokP1 = strtok(commandline , ";\n");
            //";","\n" 토큰 기준으로 명령어 구분된 첫번째 문자열.
            while (tempTokP1 != NULL) {
                // ";","\n" 토큰 기준으로 명령어를 구분하여 new_argv[i] 저장
                while (tempTokP1[0]==' ') {
                    //문자열의 시작이 ' '(띄어쓰기)일경우,' '을 없앰.
                    tempTokP1 = &tempTokP1[1];
                }
                new_argv[i] = tempTokP1;
                //new_argv[0]부터 new_argv[실행할 명령어 갯수-1]
                //";"와"\n" 로 나눠진 명령어들을 new_argv[i]에 저장
                i++;
                tempTokP1 = strtok(NULL , ";\n");
                //";","\n"로 구분된 지점의 다음지점부터 ";""\n"로 구분
            }
            //이과정으로인해 커맨드 한줄이 실행되어야 할 명령어로 나눠지게됨.
            //ex)[prompt> ls ; pwd ; ls -alf ; echo helloWorld!] ==>>
            //new_argv[0] = "ls " , new_argv[1] = "pwd " ,
            //new_argv[2] = "ls -alf "
            //new_argv[3] = "echo helloWorld!"    ,    i = 4;
      
            for (j = 0 ; j < i ; j++) {
                //" " 토큰 기준으로 또 나눔, i(커맨드의 갯수)만큼 실행
                pid = fork(); //자식프로세스 생성
                if (pid < 0) {//자식프로세서 생성 실패 일때
                    printf("fork error!! \n");
                } else if (pid == 0){//자식프로세서 일때
                    k = 0;//k값 초기화.
                    tempTokP2 = strtok(new_argv[j]," ");
                    //' '토큰 기준으로 구분된 처번째 문자열
                    while (tempTokP2 != NULL) {
                        //' ' 토큰 기준으로 스트링을 구분하여 execparam[k] 에 저장
                        execparam[k] = tempTokP2;
                        //execparam[0]부터 execparam[명령어 단어갯수-1]까지 저장
                        printf("%s ",execparam[k]);//실행할 명령어를 프린트함.
                        k++;
                        tempTokP2 = strtok(NULL," ");
                        //" "로 구분된 지점의 다음지점부터 " "로 구분
                    }
                    printf("\n");
                    execparam[k] = NULL;
                    //execvp()함수의 두번째 인자 마지막 인덱스는 NULL 이어야한다.
                    k = 0;//변경된 k 값을 다시 0으로 초기화
                    //이과정으로인해 실행할 명령어가 띄어쓰기 단위로 나누어짐.
                    //execparam은 execvp()함수의 매개변수.
                    //ex): new_argv[i] = "ls -alf " ==>>
                    //execparam[0] = "ls" , execparam[1] = "-alf"
                    //execparam[2] = NULL , k = 2;
                    
                    execvp(execparam[0],execparam);//명령어 실행
                    ///////////////////////////////////////////////////
                    //execvp() 함수가 잘 실행될경우 밑에 코드는 실행되지 않는다.////
                    //밑에 코드는 execvp()함수가 실행되지 않았을경우 에러메세지를 내보내고
                    //프로세스를 종료한다
                    printf("error at %dth child process execution!!\n",j+1);
                    printf("you would rather check your command line once again..\n");
                    printf("I am gonna terminate this process..\n");
                    exit(1);
                } else { //  부모프로세서 일때 자식 프로세스가 끝나기 기다림
                    wait(&pid);
                    //printf("< %dth child process ended >\n\n",j+1);
                }
            }
        }
    } else { //배치파일이 있는경우. [./shell batchfile]로 실행했을때,
        //매커니즘은 배치파일 없을때와 동일
        //argc==2 , argv[0] = "./shell" , argv[1] = "batchfile"
        printf("prompt> Your Batchfile's name is ""%s"" \n", argv[1]);
        //배치파일 이름을 프린트함.
        fp = fopen(argv[1],"r");//배치파일을 읽기모드로 열고
        if (fp == NULL) { //파일이 없는 경우 에러메세지 & 쉘종료
            printf("error!! please check your batchfile\n");
            return 0;
        }
        while (fgets(commandline, 1024, fp) != NULL) {
            //배치파일의 끝에 도달할때까지 반복
            printf("%s",commandline);//파일에서 읽어온 한줄 프린트
            if (strcmp(commandline,"quit\n") == 0) {
                //배치파일안에서 읽어온 한줄이 "quit"일경우 쉴종료
                printf("Shell is terminated\n");
                return 0;
            }
            i = 0;//i값 초기화
            tempTokP1 = strtok(commandline , ";\n");
            //";","\n" 토큰 기준으로 명령어 구분된 첫번째 문자열.
            while (tempTokP1 != NULL) {
                // ";","\n" 토큰 기준으로 명령어를 구분하여 new_argv[i] 저장
                while (tempTokP1[0] == ' ') {
                    //문자열의 시작이 ' '(띄어쓰기)일경우,' '을 없앰.
                    tempTokP1 = &tempTokP1[1];
                }
                new_argv[i] = tempTokP1;
                //new_argv[0]부터 new_argv[실행할 명령어 갯수-1]
                i++;
                tempTokP1 = strtok(NULL , ";\n");
                //";","\n"로 구분된 지점의 다음지점부터 ";""\n"로 구분
            }
            //이과정으로인해 배치파일에서 읽어온 커맨드 한줄이 실행되어야 할 명령어로 나눠지게됨.
            //ex)[ls ; pwd ; ls -alf ; echo helloWorld!] ==>>
            //new_argv[0] = "ls " , new_argv[1] = "pwd " ,
            //new_argv[2] = "ls -alf "
            //new_argv[3] = "echo helloWorld!"    ,    i = 4;
            
            for (j = 0 ; j < i ; j++) {
                //" " 토큰 기준으로 또 나눔, i(커맨드의 갯수)만큼 실행
                pid = fork();//자식프로세서 생성
                if (pid < 0) {//자식프로세서 생성 실패일때
                    printf("fork error!! \n");
                } else if(pid == 0) {
                    //자식프로세서 일때
                    k = 0;//k값 초기화.
                    tempTokP2 = strtok(new_argv[j]," ");
                    //' '토큰 기준으로 구분된 처번째 문자열
                    while(tempTokP2 != NULL){
                        //' ' 토큰 기준으로 스트링을 구분하여 execparam[k] 에 저장
                        execparam[k] = tempTokP2;
                        //execparam[0]부터 execparam[명령어 단어갯수-1]까지 저장
                        printf("%s ",execparam[k]);//실행할 명령어를 프린트함.
                        k++;
                        tempTokP2 = strtok(NULL," ");
                        //" "로 구분된 지점의 다음지점부터 " "로 구분
                    }
                    printf("\n");
                    execparam[k] = NULL;
                    //execvp()함수의 두번째 인자 마지막 인덱스는 NULL 이어야한다.
                    k = 0;//변경된 k 값을 다시 0으로 초기화
                    //이과정으로인해 실행할 명령어가 띄어쓰기 단위로 나누어짐.
                    //execparam은 execvp()함수의 매개변수.
                    //ex): new_argv[i] = "ls -alf " ==>>
                    //execparam[0] = "ls" , execparam[1] = "-alf"
                    //execparam[2] = NULL , k = 2;
                    
                    execvp(execparam[0],execparam);//명령어 실행
                    /////////////////////////////////////////////////////
                    ///execvp() 함수가 잘 실행될경우 밑에 코드는 실행되지 않는다.
                    ///밑에 코드는 execvp()함수가 실행되지 않았을경우
                    ///에러메세지를 내보내고 프로세스를 종료한다
                    printf("error at %dth child process execution!!\n",j+1);
                    printf("you would rather check your command line once again..\n");
                    printf("I am gonna terminate this process..\n");
                    exit(1);
                    
                }
                else{ //  부모프로세서 일때 자식 프로세스가 끝나기 기다림
                    wait(&pid);
                    //printf("< %dth child process ended >\n\n",j+1);
                }
            }
        }
        fclose(fp);// 배치파일 닫음
        printf("batchfile closed\n");
        printf("Shell is terminated\n");
        return 0; //쉘종료
    }
}
