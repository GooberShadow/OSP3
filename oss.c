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
#include <signal.h>

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

const int DEBUG = 0;

//MORE GLOBALS

int terminateTime = 5;

//Dynamic array
int* pidArray;
int pidArraySize = 0;

//Shared Memory control
struct shmid_ds shmSemCtl;
struct shmid_ds shmMsgCtl;
struct shmid_ds shmClockCtl;	
	
//Shared memory ids
int shmSemID = 0;
int shmMsgID = 0;
int shmClockID = 0;

//Func Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);
sem_t* createShmSemaphore(key_t* key, size_t* size, int* shmid);
int* createShmMsg(key_t* key, size_t* size, int* shmid);
int* createShmLogicalClock(key_t* key, size_t* size, int* shmid);
void cleanupSharedMemory(int* shmid, struct shmid_ds* ctl);

//SIGNALS
void interruptSignalHandler(int sig);
void timeSignalHandler(int sig);

//MAIN
int main(int argc, char* argv[])
{
	
	//command line args
	int maxChildren = 5;
	char* logFileName = "log.txt";

	//Iterator
	int i;

	//Process vars
	int status;
	int totalProcesses = maxChildren - 1;
	int totalProcessesWaitedOn = 0;
	pid_t pid = 0;

	//Shared memory keys
	key_t shmSemKey = SEM_KEY;
	key_t shmMsgKey = MSG_KEY;
   	key_t shmClockKey = CLOCK_KEY;

	
	//Shared memory sizes
    	size_t shmSemSize = sizeof(sem_t);
    	size_t shmMsgSize = 2 * sizeof(int);
    	size_t shmClockSize = 2 * sizeof(int);	

	
	//Shared mem pointers
    	sem_t* semPtr = NULL;
    	int* shmMsgPtr = NULL;
    	int* shmClockPtr = NULL;

	//Getopts
	handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);
	
	//File handling
	FILE* logFileHandler = NULL;

	logFileHandler = fopen(logFileName, "w+");
	fclose(logFileHandler);


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

	signal(SIGINT, interruptSignalHandler);
	signal(SIGALRM, timeSignalHandler);

	//Spawn a fan of maxChildren # of processes
	for(i = 1; i < maxChildren; i++)
	{
		pid = fork();

		//Store child into array
		if(pid > 0)
		{
			pidArraySize++;
			pidArray = realloc(pidArray, pidArraySize);
			pidArray[pidArraySize - 1] = pid;
		}

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

	//ALARM
	alarm(terminateTime);

	//sleep(1);

	//WAIT FOR EACH CHILD
	int exitedPID;
	while(1)
	{
		//sleep(1);
		//lock
		sem_wait(semPtr);

		//increment the clock
		int* tempClockPtr = shmClockPtr;
		//Tickrate
        	*tempClockPtr += 1000;
        	if(*tempClockPtr >= 1000000000) 
		{
            		*(tempClockPtr + 1) += 1;
            		*tempClockPtr = 0;
        	}

		//Check Message status
        	int isMessage = 0;
        	int* tempMsgPtr = shmMsgPtr;
        	//fprintf(stderr, "OSS: msgNano = %d, msgSec = %d\n",*tempMsgPtr,*(tempMsgPtr + 1) );
        	
		if(*tempMsgPtr != 0 || *(tempMsgPtr + 1) != 0) 
		{
            		//fprintf(stderr, "OSS found a message\n");
            		isMessage = 1;
        	}

        	if(isMessage) 
		{
            		exitedPID = wait(&status);

			//Array stuff
			int deadProcessIndex = -1;
			for( i = 0; i < pidArraySize; ++i)
			{
				if(exitedPID == pidArray[i])
				{
					deadProcessIndex = i;
					pidArray[i] = 0;
					break;
				}
			}

			logFileHandler = fopen(logFileName, "a");
			fprintf(logFileHandler, 
			"Master: Child %d is terminating at my time %ds.%dns because it reached %ds.%dns in the child process\n", exitedPID, *(shmClockPtr + 1), *shmClockPtr, *(shmMsgPtr + 1), *shmMsgPtr);

			fclose(logFileHandler);
			totalProcessesWaitedOn++;
			

			//Reset msg
			*tempMsgPtr = 0;
			*(tempMsgPtr + 1) = 0;

			if(DEBUG)
			{
				fprintf(stderr, "Child %d -- exit status: %d\n", exitedPID, WEXITSTATUS(status));

			}

			//Spawn new children as old ones end
			int newProcessId;
			if(totalProcesses < 100)
			{
				totalProcesses++;
				newProcessId = fork();
				if(newProcessId == 0)
				{
					execl("./usrPs", "usrPs", (char *) NULL);
				}
				
				pidArray[deadProcessIndex] = newProcessId;
			}
			
		}

		if(!isMessage && totalProcesses >= 100 && totalProcessesWaitedOn == totalProcesses)
		{
			sem_post(semPtr);
			break;
		}


		
			
			//unlock
			sem_post(semPtr);

	}


	//Remove shared mem
	if(pid > 0)
	{
		cleanupSharedMemory(&shmMsgID, &shmMsgCtl);
        	cleanupSharedMemory(&shmClockID, &shmClockCtl);
        	cleanupSharedMemory(&shmSemID, &shmSemCtl);
	}

	free(pidArray);

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
	if(flagArray[HELP_FLAG])
	{
		printf("HELP MESSAGE:\n");
		printf("-h for help message:\n");
		printf("-s x for  setting max child processes:\n");
		printf("-l filename for log file used:\n");
		printf("-t z for time in real seconds for termination:\n");
		exit(0);
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

void interruptSignalHandler(int sig)
{
	shmctl(shmMsgID, IPC_RMID, &shmMsgCtl);
	shmctl(shmClockID, IPC_RMID, &shmClockCtl);
	shmctl(shmSemID, IPC_RMID, &shmSemCtl);
	printf("OSS ended as it caught ctrl-c signal\n");

	free(pidArray);

	exit(0);
}

void timeSignalHandler(int sig)
{
	int i = 0;
	
	shmctl(shmMsgID, IPC_RMID, &shmMsgCtl);
	shmctl(shmClockID, IPC_RMID, &shmClockCtl);
	shmctl(shmSemID, IPC_RMID, &shmSemCtl);
	printf("OSS timed out after %d seconds.\n", terminateTime);
	
	for(i = 0; i < pidArraySize; ++i)
	{
		if(pidArray[i] != 0)
		{
			kill(pidArray[i], SIGQUIT);
		}
	}

	free(pidArray);

	exit(0);
}
