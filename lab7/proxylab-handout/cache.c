/****************************************************************************** 
 * 
 * Proxy lab
 * Min Xu
 * andrewID: minxu
 *
 * This is an LRU cache for caching web contents forwarded back from servers
 * to clients. It is implementd by a FIFO queue using doubly linked list. Total
 * cache size is 1 MB and single web data size is 1 KB. Anything larger than 
 * 1 KB will not be cached. If cache size exceeds 1 MB, the leaset recently 
 * used content will be replaced. 
 * 
 * ***************************************************************************/

#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define APPROX_LRU 1 //for enabling approximate LRU cache

/* initCache - initialize the cache in the heap */
queue *initCache() {

	queue *init = (queue *)Calloc(1, sizeof(queue));

	init->head = 0;
	init->tail = 0;
	init->cacheSize = 0;
	init->readcnt = 0;
	Sem_init(&(init->readSem), 0, 1);
	Sem_init(&(init->writeSem), 0, 1);

	return init;
}

/* pushCache - based on given indata and inurl, allocate a new cache object 
 * store it in the cache as the new head, remove LRU object if neccessary
 * in order to have enough cache space */
void pushCache(char *indata, size_t dataSize, char *inurl, size_t urlSize, \
                                                           queue *cacheQueue) {

	P(&cacheQueue->writeSem); //lock writers

	while(dataSize + cacheQueue->cacheSize > MAX_CACHE_SIZE) {
		popCache(cacheQueue); //pop cached objects until cache is big enough
	}

	/* allocate memory for new object pointer */
	object *newObj = (object *)Calloc(1, sizeof(object));

	/* allocate memory for data and url and copy them */
	newObj->data = (char *)Calloc(1, dataSize); 
	newObj->durl = (char *)Calloc(1, urlSize);
	memcpy(newObj->data, indata, dataSize);
	memcpy(newObj->durl, inurl, urlSize); 
	newObj->dsize = dataSize; 
	
	//increment cache size
	cacheQueue->cacheSize += newObj->dsize;

	/* insert as the new head */
	newObj->prev = NULL;
	newObj->next = cacheQueue->head;

	/* configure the head and tail of the queue */
	if(cacheQueue->head == NULL) { // if this is the first object
		cacheQueue->head = newObj;
		cacheQueue->tail = newObj;
	}
	else { //already a head object
		cacheQueue->head->prev = newObj;
		cacheQueue->head = newObj;
	}
	
	V(&cacheQueue->writeSem); //unlock writers
}

/* popCache - remove the tail object of the cache
 * popCache does not need mutex lock since it is only called
 * during pushCache and lock is already in place */
void popCache(queue *cacheQueue) {
	object *temp = cacheQueue->tail;
	
	//decrement cache size
	cacheQueue->cacheSize -= temp->dsize;
	
	//set the new tail, free the old tail
	cacheQueue->tail = cacheQueue->tail->prev;
	cacheQueue->tail->next = NULL;
	Free(temp->data);
	Free(temp->durl);
	Free(temp);
}

/* searchCache - look for object with the same url string, return the
 * object if found, return null otherwise. this object will be the MRU 
 * object, which will be set as the new head */
object *searchCache(char *inurl, queue *cacheQueue) {
	
	/* first readers-writers, lock for read only */
	P(&cacheQueue->readSem);
	cacheQueue->readcnt++;
	if(cacheQueue->readcnt == 1) //first in  
		P(&cacheQueue->writeSem);
	V(&cacheQueue->readSem);
	
	object *curr = cacheQueue->head; //start from the head

	while(curr != NULL) { //go through the queue
		if(!strcmp(curr->durl, inurl)) //if found url, break
			break;
		curr = curr->next;
	}
	
	/* first readers-writers, unlock */
	P(&cacheQueue->readSem);
	cacheQueue->readcnt--;
	if(cacheQueue->readcnt == 0) //last out 
		V(&cacheQueue->writeSem);
	V(&cacheQueue->readSem);
	
	/* if APPROX_LRU enabled, this will be approximately LRU
	 * but enbale true concorrent reading in this function */
	if(APPROX_LRU) { 
		return curr;
	}
	else { //if APPROX_LRU not enabled, this will be the true LRU cache 
		/* lock for writers, need to modify the queue */
		P(&cacheQueue->writeSem);

		if(curr == NULL) { //path not found, return null
			V(&cacheQueue->writeSem);
			return NULL;
		}

		if(curr != cacheQueue->head) { //if not the head, bring it to head
			//configure the neighbours of curr
			if(curr->next != NULL)
				curr->next->prev = curr->prev;
			//curr's prev has to be non-null since it is not the head
			curr->prev->next = curr->next;
		
			//bring it as the new head
			curr->prev = NULL;
			curr->next = cacheQueue->head;
			cacheQueue->head->prev = curr;
			cacheQueue->head = curr;
		} 

		/* unlock for writers */
		V(&cacheQueue->writeSem);

		return cacheQueue->head; 
	}
}
