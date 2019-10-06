//user process

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#include "share.h"

const int DEBUG = 1;

//Function prototypes
sem_t* getSemaphore(key_t* key, size_t* size, int* shmid);
int* getShmMsg(key_t* key, size_t* size, int* shmid);
int* getShmLogicalClock(key_t* key, size_t* size, int* shmid);

//MAIN
int main(int argc, char* argv[])
{
	//Iterator
	int i;

	//Shared Memory keys
    	key_t shmSemKey = SEM_KEY;
    	key_t shmMsgKey = MSG_KEY;
    	key_t shmClockKey = CLOCK_KEY;

	//Shared memory ids
    	int shmSemID = 0;
    	int shmMsgID = 0;
    	int shmClockID = 0;

	//Shared Memory size
    	size_t shmSemSize = sizeof(sem_t);
    	size_t shmMsgSize = 2 * sizeof(int);
    	size_t shmClockSize = 2 * sizeof(int);

	//Shared memory control
	struct shmid_ds shmSemCtl;
    	struct shmid_ds shmMsgCtl;
    	struct shmid_ds shmClockCtl;
    	int rtrn;

	//Shared mem ptr
    	sem_t* semPtr = NULL;
    	int* shmMsgPtr = NULL;
    	int* shmClockPtr = NULL;


    	if(DEBUG) 
	{
        	fprintf(stderr, "Child:%d, says hello to parent:%d\n", getpid(), getppid());
        	fprintf(stderr, "shmMsgSize: %ld\n", shmMsgSize);
    	}

	//Setup shared mem
    	semPtr = getSemaphore(&shmSemKey, &shmSemSize, &shmSemID);
    	shmMsgPtr = getShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);
    	shmClockPtr = getShmLogicalClock(&shmClockKey, &shmClockSize, &shmClockID);

	//Test ptint
	sem_wait(semPtr);
	fprintf(stderr, "MSGArray: ");
    	for(i = 0; i < shmMsgSize / sizeof(int); ++i) 
	{
        	fprintf(stderr, "%d", *shmMsgPtr++);
    	}
    	fprintf(stderr, "\n");
    	sem_post(semPtr); //unlock

    	return 100;
}

//Functions
int* getShmMsg(key_t* key, size_t* size, int* shmid) 
{
	//Fetch the schmid
    	*shmid = shmget(*key, *size, 0777);
    	if(*shmid < 0) 
	{
        	perror("ERROR:usrPs:shmget failed(msg)");
        	exit(1);
    	}

	//Attach to segment
    	int* temp = (int*)shmat(*shmid, 0, 0);
    	if(temp == (int*) -1) 
	{
        	perror("ERROR:usrPs:shmat failed(msg)");
        	exit(1);
    	}

    	return temp;
}

int* getShmLogicalClock(key_t* key, size_t* size, int* shmid)
{
	//Fetch the shmid
	*shmid = shmget(*key, *size, 0777);
    	if(*shmid < 0) 
	{
        	perror("ERROR:usrPs:shmget failed(clock)");
        	exit(1);
    	}

	//Attach to segment
	int* temp = (int*)shmat(*shmid, 0, 0);
    	if(temp == (int*) -1) 
	{
        	perror("ERROR:usrPs:shmat failed(clock)");
        	exit(1);
    	}

    	return temp;
}

sem_t* getSemaphore(key_t* key, size_t* size, int* shmid) 
{
	//Get the shmid
    	*shmid = shmget(*key, *size, 0777);
    	if(*shmid < 0) 
	{
        	perror("ERROR:usrPs:shmget failed(semaphore)");
        	exit(1);
    	}

	//Attach the segment
    	sem_t* temp = (sem_t*)shmat(*shmid, 0, 0);
    	if(temp == (sem_t*) -1) 
	{
        	perror("ERROR:usrPs:shmat failed(semaphore)");
        	exit(1);
    	}

    	return temp;

}

