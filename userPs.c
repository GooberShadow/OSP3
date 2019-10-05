//user process

#include <stdio.h>
#include "share.h"

void processTheChild(size_t size);

int main(int argc, char* argv[])
{
	key_t key = atoi(argv[1]);
	printf("THIS IS KEY: %d\n", key);
	printf("This is userPs.c\n");
	
	int maxChildren = 0;
	maxChildren = atoi(argv[2]);
	size_t size;
	size = sizeof(int) + sizeof(long unsigned int) + sizeof(int) * maxChildren;

	processTheChild(size);
	return 0;
}


void processTheChild(size_t size)
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
		return;
	}

	 cshmPtr = (char*)shmat(cshmid, 0, 0);
   	 if(cshmPtr == (char*) -1) 
	 {
       	 	perror("ERROR:usrPs:shmat failed");
       		return;
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


