/****************************************************************************** 
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

/* object is struct for indivisual web content marked by URL. It has web 
 * content data, url, data size and its next and prvious objects in the 
 * FIFO queue */
typedef struct object {
	char *data;
	char *durl;
	size_t dsize;
	struct object *next;
	struct object *prev;
} object;

/* queue is struct for holding global information about the cache. It has
 * the head object of the queue, tail object of the queue, total cache size,
 * read and write semaphores(mutexes) and a reader's counter for implementing
 * first readers-writers problem */
typedef struct queue {
	object *head;
	object *tail;
	size_t cacheSize;
	sem_t readSem;
	sem_t writeSem;
	unsigned int readcnt;
} queue;

/* function prototypes for cache.c */
queue *initCache();

void pushCache(char *indata, size_t dataSize, char *inurl, size_t urlSize, \
                                                           queue *cacheQueue);

void popCache(queue *cacheQueue);

object *searchCache(char *inpath, queue *cacheQueue);
