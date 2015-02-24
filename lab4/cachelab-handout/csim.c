/* Min Xu
 * andrewID: minxu
 * email: minxu@andrew.cmu.edu
 * This program simulate the behavior of a cache
 * It will generate counts of {hit, miss, eviction}
 * based on inputs of cache size and valgrind trace file */


#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#define ADDR_SIZE 64
#define MAX_TRACE_INPUTS 100
#define OPERATION_BUFFER_SIZE 2

/*each line has info of valid bit, tag ID and last access time*/
typedef struct {
  int valid;
  unsigned long long tag;
  int accessTime;
} Lines;  

/*each set has info of associative lines info*/
typedef struct {
  Lines *lines;
} Sets;

/*cache has info of sets and the size of sets and lines*/
typedef struct {
  int LinesSize;
  int SetsSize;
  Sets *sets;
} Cache;

/*parameters from command lines*/
typedef struct {
  int S;
  int E;
  int B;
  int V;
  char *fileName;
} Commands;

/*count hit, miss and eviction in operation*/
typedef struct {
  int Hit;
  int Miss;
  int Eviction;
} Counts;

/*define global varibles for results*/
int hits = 0;
int misses = 0;
int evictions = 0;

/*start the operations, return counts of hit/miss/eviction*/
Counts startOp(Commands cmdOpt, unsigned long long addr, Cache *cache);

/*get the tag ID based on address */
unsigned long long getTagID(int s, int b, unsigned long long addr);

/*get the set ID based on address*/
int getSetID(int s, int b, unsigned long long addr);

/*cache initialization, return an initialized 
 cache with required size based on s size and E size*/
Cache* cacheInit(int s, int e);

/*free cache and sets that have been allocated*/
void freeCache(Cache* cache);


/*get the operation from parsing command, return a Commands struct*/
Commands getOpt(int argc, char** argv);

/*get the trace info of operation and address, call the operation
  function startOp based on inputs */
void parseTrac(Commands cmdOpt);

int main(int argc, char** argv) {
  Commands cmdOpt = getOpt(argc, argv);
  parseTrac(cmdOpt);
  printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
  printSummary(hits, misses, evictions);
  return 0;
}

/*get the operation from parsing command, return a Commands struct*/
Commands getOpt(int argc, char** argv) {
  int opt;
  Commands cmdOpt = {0, 0, 0, 0, NULL};
  while( -1 != (opt = getopt(argc, argv, "vs:E:b:t:"))) {
    switch(opt) {
      case 'h':
              printf("I'm printing usage info\n");
              break;
      case 'v':
              cmdOpt.V = 1;
              break;
      case 's':  
              cmdOpt.S = atoi(optarg);
              if((cmdOpt.S < 1) && (cmdOpt.S > ADDR_SIZE)) {
                printf("set size not valid\n");
                exit(-1);
              }
              break;
      case 'E':
              cmdOpt.E = atoi(optarg);
              if(cmdOpt.E < 1) { 
                printf("line size not valid\n");
                exit(-1);
              }
              break;
      case 'b':
              cmdOpt.B = atoi(optarg); 
              if((cmdOpt.B < 1) && (cmdOpt.B > ADDR_SIZE)) {
              printf("line size not valid\n");
              exit(-1);
              }
              break;
      case 't':
              cmdOpt.fileName = optarg;
              break;
      default:
              printf("invalid commands\n");
              break;     
    }
  }
  return cmdOpt;
}

/*get the trace info of operation and address, call the operation
  function startOp based on inputs */
void parseTrac(Commands cmdOpt) {
  unsigned long long addr;
  char traceInputs[MAX_TRACE_INPUTS];
  char firstTwoChar[OPERATION_BUFFER_SIZE];
  char operation;
  int byteSize;
  /*miss/hit/eviction counts for different operations */
  Counts countLoad = {0, 0, 0}; 
  Counts countStore = {0, 0, 0};
  /*return the file to a pointer based on fileName*/
  FILE *traceFile = fopen(cmdOpt.fileName, "r");
  /*a pointer points to an initialized cache struct*/
  Cache *cacheRun = cacheInit(cmdOpt.S, cmdOpt.E);
  if (traceFile == NULL) {
    printf("could not open file %s\n", cmdOpt.fileName);
    exit(-1);
  }
  while(fgets(traceInputs, MAX_TRACE_INPUTS, traceFile) != NULL) {
    /*read the cotent of one file line including 
      operation, address and bytesize*/
    sscanf(traceInputs, "%2s %llx,%d", firstTwoChar, &addr, &byteSize);
    operation = firstTwoChar[0];
    /*determine the operation based on file inputs L, S, M*/
    switch(operation) {
      case 'I': 
              break;
      case 'L':
              countLoad = startOp(cmdOpt, addr, cacheRun);
              break;
      case 'S':
              countStore = startOp(cmdOpt, addr, cacheRun);
              break;
      case 'M':
              countLoad = startOp(cmdOpt, addr, cacheRun);
              countStore = startOp(cmdOpt, addr, cacheRun);                 
              break;
      default:
              printf("invalid first trace command\n");
              exit(-1);
    }
    /*print operations and results if in verbose mode*/
    if ((operation != 'I') && (cmdOpt.V == 1)) {
      printf("%c %llx,%d, %d hit, %d miss, %d eviction\n", \
             operation, addr, byteSize, countLoad.Hit+countStore.Hit, \
             countLoad.Miss+countStore.Miss, \
             countLoad.Eviction+countStore.Eviction);
    }
  }
  fclose(traceFile);
  freeCache(cacheRun); 
}

/*cache initialization, return an initialized 
 cache with required size*/
Cache* cacheInit(int s, int e) {
  int sIndex, lIndex;
  Cache* cacheNew;
  cacheNew = (Cache *) malloc(sizeof(Cache));
  cacheNew->SetsSize = 1 << s;
  cacheNew->LinesSize = e;
  cacheNew->sets = (Sets *) malloc(sizeof(Sets) * cacheNew->SetsSize);
  for (sIndex = 0; sIndex < cacheNew->SetsSize; sIndex++) {
    cacheNew->sets[sIndex].lines = \
    (Lines *) malloc(sizeof(Lines) * cacheNew->LinesSize);
    for (lIndex = 0; lIndex < cacheNew->LinesSize; lIndex++) {
      cacheNew->sets[sIndex].lines[lIndex].valid = 0;
      cacheNew->sets[sIndex].lines[lIndex].tag = 0;
      cacheNew->sets[sIndex].lines[lIndex].accessTime = 0;
    }
  }   
  return cacheNew;
}

/*free cache and sets that have been allocated*/
void freeCache(Cache* cache) {
  int sIndex;
  for(sIndex = 0; sIndex < cache->LinesSize; sIndex++) {
    free(cache->sets[sIndex].lines);
  }
  free(cache->sets);
  free(cache);
}

/*start operation based on inputs, identical for load and store,
  return the counts for hit/miss/eviction */
Counts startOp(Commands cmdOpt, unsigned long long addr, Cache *cache) {
  Counts counts = {0, 0, 0};
  int setID = getSetID(cmdOpt.S, cmdOpt.B, addr);
  unsigned long long tagID = getTagID(cmdOpt.S, cmdOpt.B, addr);
  /*indicate whether there is a hit or a evictions*/
  int hitFlag = 0, evictFlag = 1;
  /*give the index for the least recently used line*/
  int lruIndex = 0;
  int lIndex;
  /*go through each line and check valid and tag, update accessTime,
    if match, update hitFlag and zero accessTime*/
  for(lIndex = 0; lIndex < cache->LinesSize; lIndex++) {
    cache->sets[setID].lines[lIndex].accessTime++;
    if((cache->sets[setID].lines[lIndex].tag == tagID) \
        && (cache->sets[setID].lines[lIndex].valid == 1)){
      hitFlag = 1;
      cache->sets[setID].lines[lIndex].accessTime = 0;
    }
  }
  /*if hit, hit counts incease*/
  if(hitFlag == 1) {
    hits++;
    counts.Hit = 1;
  }   
  /*if not hit, miss counts increase*/
  else {
    misses++; 
    counts.Miss = 1;
    for(lIndex = 0; lIndex < cache->LinesSize; lIndex++) {
      /*if find any empty line (valid = 0), 
        operate on that and no eviction, update evictFlag*/
      if(cache->sets[setID].lines[lIndex].valid == 0) {
        cache->sets[setID].lines[lIndex].tag = tagID;
        cache->sets[setID].lines[lIndex].valid = 1;
        cache->sets[setID].lines[lIndex].accessTime = 0; 
        evictFlag = 0;
        break;
      }
      /*search for the largest accessTime, this will be the LRU line*/
      if((lIndex > 0) && \
         (cache->sets[setID].lines[lIndex].accessTime \
          > cache->sets[setID].lines[lruIndex].accessTime)) {
        lruIndex = lIndex;

      }
    }
    /*if cannot find any empty line, need to evict the LRU line*/
    if (evictFlag) {
      evictions++;
      counts.Eviction = 1;
      cache->sets[setID].lines[lruIndex].tag = tagID;
      cache->sets[setID].lines[lruIndex].accessTime = 0;
    }
  } 
  return counts;
}

/*get the tag ID based on address */
unsigned long long getTagID(int s, int b, unsigned long long addr) {
  unsigned long long tagID;
  return tagID = addr >> (s + b);
}

/*get the set ID based on address*/
int getSetID(int s, int b, unsigned long long addr) {
  int setID;
  return setID = (int)(addr >> b) & ((1 << s) - 1); 
}
