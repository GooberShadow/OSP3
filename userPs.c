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
#include <time.h>

#include "share.h"

const int DEBUG = 0;

//Shared memory ids
int shmSemID = 0;
int shmMsgID = 0;
int shmClockID = 0;

//Function prototypes
sem_t* getSemaphore(key_t* key, size_t* size, int* shmid);
int* getShmMsg(key_t* key, size_t* size, int* shmid);
int* getShmLogicalClock(key_t* key, size_t* size, int* shmid);
void setDeathTime(int* nanosec, int* sec, int* clockPtr);
void criticalSection();

//Signals
void timeSignalHandler(int sig);

//MAIN
int main(int argc, char* argv[])
{

	signal(SIGQUIT, timeSignalHandler);
	//Seed the rand
	srand(time(NULL));

	//Iterator
	int i;

	//Shared Memory keys
    	key_t shmSemKey = SEM_KEY;
    	key_t shmMsgKey = MSG_KEY;
    	key_t shmClockKey = CLOCK_KEY;


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

	//Life vars
	int deathNanosec = 0;
	int deathSec = 0;
	int isInitialized = 0;

    	if(DEBUG) 
	{
        	fprintf(stderr, "Child:%d, says hello to parent:%d\n", getpid(), getppid());
        	fprintf(stderr, "shmMsgSize: %ld\n", shmMsgSize);
    	}

	//Setup shared mem
    	semPtr = getSemaphore(&shmSemKey, &shmSemSize, &shmSemID);
    	shmMsgPtr = getShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);
    	shmClockPtr = getShmLogicalClock(&shmClockKey, &shmClockSize, &shmClockID);

	//Set lifetime of process
	sem_wait(semPtr);
	setDeathTime(&deathNanosec, &deathSec, shmClockPtr);
	sem_post(semPtr);

	//Critical section
	while(1)
	{
		//sleep(1);

		//lock
		sem_wait(semPtr); 

		//Read the clock
		int* tempClockPtr = shmClockPtr;
        	int clockNano = *tempClockPtr;
        	int clockSec = *(tempClockPtr + 1);

		//Check Message status
		int isMessage = 0;
        	int* tempMsgPtr = shmMsgPtr;
        	if(*tempMsgPtr != 0 || *(tempMsgPtr + 1) != 0) 
		{
        		isMessage = 1;
        	}

		//Exit, post, and release if it is time
        	if(clockSec > deathSec || (clockSec >= deathSec && clockNano >= deathNanosec)) 
		{
            		if(isMessage == 0) 
			{
				*shmMsgPtr = deathNanosec;
				*(shmMsgPtr + 1) = deathSec;

				if(DEBUG)
				{
					fprintf(stderr, "nano=%d, sec=%d\n", clockNano, clockSec);
				}

				sem_post(semPtr);
				exit(50);

			}
		}
		
		//unlock
		sem_post(semPtr); 
	}

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

void setDeathTime(int* nanosec, int* sec, int* clockPtr)
{
	int* temp = clockPtr;

	//Read shared memory clock
	*nanosec = *temp;
	*sec = *(temp + 1);

	//Randomly generate duration
	*nanosec += rand() % 1000000;

	//Rollover
	if(*nanosec >= 1000000000)
	{
		*nanosec = 0;
		*sec += 1;
	}
}

void timeSignalHandler(int sig)
{
	shmdt(&shmMsgID);
	shmdt(&shmClockID);
	shmdt(&shmSemID);

	exit(33);
	

}
