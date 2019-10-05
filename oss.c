//oss.c file
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
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

key_t key = 69;

//Func Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);
void processChild(size_t size);

//MAIN
int main(int argc, char* argv[])
{
	int maxChildren = 5;
	char* logFileName = "log.txt";
	int terminateTime = 5;
	int i;
	int status;
	//Shared memory vars
	int shmflg;
	int shmid;
	size_t size;
	char* shmPtr = NULL;
	int* shmIntPtr = NULL;
	struct shmid_ds shmid_ds;
	int rtrn;

	handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);

	if(DEBUG)
	{
		printf("maxChildren = %d\n", maxChildren);
		printf("logFileName = %s\n", logFileName);
		printf("terminateTime = %d\n\n", terminateTime);
		printf("Parent before Fork: %d\n", getpid());
	}

	//size will be two ints for seconds and nanosecs, and number of children
	size = sizeof(int) + sizeof(long unsigned int) + sizeof(int) * maxChildren;

	//Spawn a fan of maxChildren # of processes
	pid_t pid = 0;
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
			break;
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

	if(pid > 0)
	{
		//Create shared memory
		int ossShmFlags = IPC_CREAT | IPC_EXCL | 0777;
		shmid = shmget(key, size, ossShmFlags);
		if(shmid < 0)
		{
			perror("ERROR:oss:shmget failed");
			fprintf(stderr, "key:%d, size:%ld, flags:%d\n", key, size, ossShmFlags);
			exit(1);
		}

		//Attach the segment
		shmPtr = (char*)shmat(shmid, NULL, 0);
		shmIntPtr = (int*)shmPtr;

		//Test write to shared mem
		for(i = 0; i < size / sizeof(int); ++i)
		{
			*shmIntPtr++ = i - 2;
		}
	}

	sleep(2);

	//CHILD
	if(pid == 0)
	{
		sleep(1);
		if(DEBUG)
		{
			printf("Child:%d, says hello to parent:%d\n", getpid(), getppid());
		}
		//SEND CHILD TO usrPs
		char* args[] = {"./usrPs", "69", "5", NULL};
		execv(args[0], args);
		//processChild(size);
		//exit(0);
	}

	for(i = 1; i < maxChildren; i++)
	{
		sleep(1);
		wait(&status);
		if(DEBUG)
		{
			printf("%d: exit status : %d\n", i, WEXITSTATUS(status));
		}
	}

	if(pid > 0)
	{
		int cmd = IPC_RMID;
		rtrn = shmctl(shmid, cmd, &shmid_ds);
		if(rtrn == -1)
		{
			perror("ERROR:oss:shmctl failed");
			exit(1);
		}
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

void printIntArray(int* arr, int size)
{
	int i;
	for(i = 0; i < size; ++i)
	{
		printf("%d ", arr[i]);
	}
	printf("\n");
}

void processChild(size_t size)
{
    key_t ckey = 69;
    int cshmid;
    char* cshmPtr = NULL;
    int* cshmIntPtr = NULL;

    sleep(3);
    //Fetch the segment id
    cshmid = shmget(ckey, size, 0777);
	if(cshmid < 0)
	{
		perror("ERROR:usrPs:shmget failed");
		fprintf(stderr, "size = %ld\n", size);
		exit(1);
	}

	 cshmPtr = (char*)shmat(cshmid, 0, 0);
   	 if(cshmPtr == (char*) -1) 
	 {
       	 	perror("ERROR:usrPs:shmat failed");
       		 exit(1);
   	 }
 	 cshmIntPtr = (int*)cshmPtr;

	//Read the shared memory
	fprintf(stderr, "Child %d reads: ", getpid());
    	int i;
   	for(i = 0; i < size / sizeof(int); ++i) 
	{
       	 	fprintf(stderr, "%d ", *cshmIntPtr++);
   	}
    	fprintf(stderr, "\n");

}
