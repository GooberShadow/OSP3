//oss.c file
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>
#include <semaphore.h>

#include "share.h"

//Globals
enum FLAGS 
{
	HELP_FLAG,
	SPAWN_FLAG,
	LOG_FLAG,
	TIME_FLAG,
	TOTAL_FLAGS
};

const int DEBUG = 1;

//Func Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);
sem_t* createShmSemaphore(key_t* key, size_t* size, int* shmid);
int* createShmMsg(key_t* key, size_t* size, int* shmid);
int* createShmLogicalClock(key_t* key, size_t* size, int* shmid);
void cleanupSharedMemory(int* shmid, struct shmid_ds* ctl);

//MAIN
int main(int argc, char* argv[])
{
	//command line args
	int maxChildren = 5;
	char* logFileName = "log.txt";
	int terminateTime = 5;

	//Iterator
	int i;

	//Process vars
	int status;
	pid_t pid = 0;

	//Shared memory keys
	key_t shmSemKey = SEM_KEY;
	key_t shmMsgKey = MSG_KEY;
   	key_t shmClockKey = CLOCK_KEY;

	//Shared memory ids
	int shmSemID = 0;
    	int shmMsgID = 0;
    	int shmClockID = 0;
	
	//Shared memory sizes
    	size_t shmSemSize = sizeof(sem_t);
    	size_t shmMsgSize = 2 * sizeof(int);
    	size_t shmClockSize = 2 * sizeof(int);	

	//Shared Memory control
    	struct shmid_ds shmSemCtl;
    	struct shmid_ds shmMsgCtl;
    	struct shmid_ds shmClockCtl;	
	
	//Shared mem pointers
    	sem_t* semPtr = NULL;
    	int* shmMsgPtr = NULL;
    	int* shmClockPtr = NULL;

	//Getopts
	handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);


	if(DEBUG)
	{
		fprintf(stderr, "maxChildren = %d\n", maxChildren);
		fprintf(stderr, "logFileName = %s\n", logFileName);
		fprintf(stderr, "terminateTime = %d\n\n", terminateTime);
		fprintf(stderr, "Parent before Fork: %d\n", getpid());
		fprintf(stderr, "sem_t size: %ld\n", sizeof(sem_t));
	}

	//Setup shared mem with semaphore
    	semPtr = createShmSemaphore(&shmSemKey, &shmSemSize, &shmSemID);
    	shmMsgPtr = createShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);
    	shmClockPtr = createShmLogicalClock(&shmClockKey, &shmClockSize, &shmClockID);

	//Spawn a fan of maxChildren # of processes
	for(i = 1; i < maxChildren; i++)
	{
		pid = fork();

		//fork error
		if(pid < 0)
		{
			perror("ERROR oss failed to fork");
			exit(1);
		}

		//process child only
		if(pid == 0)
		{
			execl("./usrPs", "usrPs", (char *) NULL);
			exit(55);
		}

		//Process parent only
		if(pid > 0)
		{
			if(DEBUG)
			{
				fprintf(stderr, "onPass:%d, Parent created child process: %d\n", i, pid);
			}
		}

	}

	//WAIT FOR EACH CHILD
	int exitedPID;


	for(i = 1; i < maxChildren; i++)
	{
        	exitedPID = wait(&status);
        	sem_wait(semPtr);
        	if(DEBUG) fprintf(stderr, "Child %d -- exit status: %d\n", exitedPID, WEXITSTATUS(status));
        	sem_post(semPtr);
	}

	//Remove shared mem
	if(pid > 0)
	{
		cleanupSharedMemory(&shmMsgID, &shmMsgCtl);
        	cleanupSharedMemory(&shmClockID, &shmClockCtl);
        	cleanupSharedMemory(&shmSemID, &shmSemCtl);
	}

	return 0;
}



//Functions
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime)
{
	int i;
	int flagArray[TOTAL_FLAGS];

	for(i = 0; i < TOTAL_FLAGS; i++)
	{
		flagArray[i] = 0;
	}

	if(DEBUG)
	{
		printf("flagArray initialized to:\n");
		printIntArray(flagArray, TOTAL_FLAGS);
	}

	//Parse the arguments from command line
	int arg = -1;
	while((arg = getopt(argc, argv, "hs:l:t:")) != -1)
	{
		switch(arg)
		{
			case 'h':
				flagArray[HELP_FLAG] = 1;
				break;
			case 's':
				flagArray[SPAWN_FLAG] = 1;
				*maxChild = atoi(optarg);
				break;
			case 'l':
				flagArray[LOG_FLAG] = 1;
				*logFile = optarg;
				break;
			case 't':
				flagArray[TIME_FLAG] = 1;
				*termTime = atoi(optarg);
				break;
			case '?':
				printf("ERROR:oss:Invalid CLI arguments\nProgram Exited");
				exit(1);
				break;
		}
	}
}

//DONT NEED THIS
void printIntArray(int* arr, int size)
{
	int i;
	for(i = 0; i < size; ++i)
	{
		printf("%d ", arr[i]);
	}
	printf("\n");
}


sem_t* createShmSemaphore(key_t* key, size_t* size, int* shmid) 
{
	int ossShmFlags = IPC_CREAT /* | IPC_EXCL  */| 0777;

	//Allocate shared mem
    	*shmid = shmget(*key, *size, ossShmFlags);
    	if(*shmid < 0) 
	{
    		perror("ERROR:oss:shmget failed(semaphore)");
    		fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        	exit(1);
    	}

	//Set pointer
	sem_t* temp = (sem_t*)shmat(*shmid, NULL, 0);
    	if(temp == (sem_t*) -1) 
	{
        	perror("ERROR:oss:shmat failed(semaphore)");
        	exit(1);
    	}

	//Initialize sem
	if(sem_init(temp, 1, 1) == -1)
	{
		perror("ERROR:oss:sem_init failed");
		exit(1);
	}

	return temp;
}

int* createShmMsg(key_t* key, size_t* size, int* shmid) 
{
	int ossShmFlags = IPC_CREAT /* | IPC_EXCL  */| 0777;

	//Allocate shared mem
	*shmid = shmget(*key, *size, ossShmFlags);
	if(*shmid < 0) 
	{
        	perror("ERROR:oss:shmget failed(msg)");
        	fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        	exit(1);
	}

	//Set pointer
    	int* temp = (int*)shmat(*shmid, NULL, 0);
    	if(temp == (int*) -1) 
	{
        	perror("ERROR:oss:shmat failed(msg)");
        	exit(1);
	}

	return temp;
}

int* createShmLogicalClock(key_t* key, size_t* size, int* shmid) 
{
    	int ossShmFlags = IPC_CREAT /* | IPC_EXCL  */| 0777;

	//Allocate mem
    	*shmid = shmget(*key, *size, ossShmFlags);
    	if(*shmid < 0) 
	{
        	perror("ERROR:oss:shmget failed(clock)");
        	fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        	exit(1);
    	}

	//Set pointer
    	int* temp = (int*)shmat(*shmid, NULL, 0);
    	if(temp == (int*) -1) 
	{
        	perror("ERROR:oss:shmat failed(clock)");
        	exit(1);
    	}

	return temp;

}

void cleanupSharedMemory(int* shmid, struct shmid_ds* ctl) 
{
    	int cmd = IPC_RMID;
    	int rtrn = shmctl(*shmid, cmd, ctl);
        if(rtrn == -1) 
	{
            	perror("ERROR:oss:shmctl failed");
            	exit(1);
        }
}

