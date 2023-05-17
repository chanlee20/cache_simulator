#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <getopt.h>
#include "cachelab.h"

typedef unsigned long long int mem_addr_t;

struct Line
{ 
  bool valid;
  long tag;
  int lru;
  bool dirty;
};

struct Set
{ 
  struct Line *lines;
};

struct Cache
{ 
  struct Set *sets;
};

enum{
  zero,
  opIndex,
  invalid = -1,
  toAddressIndex = 3,
  lengthOfLine = 100
};

enum
{ 
  success,
  failure
};

void print_help()
{ 
  printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
  printf("Options:\n");
  printf("-h: Optional help flag that prints usage info.\n");
  printf("-v: Optional verbose flag that displays trace info.\n");
  printf("-s <s>: Number of set index bits (S = 2^s is the number of sets).\n");
  printf("-E <E>: Associativity (number of lines per set).\n");
  printf("-b <b>: Number of block offset bits (B = 2^b is the block size).\n");
  printf("-t <tracefile>:  Name of the valgrind trace to replay.\n");
}


//dynamically allocate memory for the cache
int buildCache(int S, int E, struct Cache *cache)
{
  cache->sets = (struct Set *)malloc(S * sizeof(struct Set));
  if (cache->sets == NULL)
  {
    printf("Failed to allocate memory for cache.");
    return failure;
  }
  for (int i = 0; i < S; ++i)
  {
    cache->sets[i].lines = (struct Line *)malloc(E * sizeof(struct Line));
    if (cache->sets[i].lines == NULL)
    {
      printf("Failed to allocate memory for cache.");
      return failure;
    }
    for (int j = 0; j < E; ++j)
    {
      cache->sets[i].lines[j].valid = false;
      cache->sets[i].lines[j].tag = zero;
      cache->sets[i].lines[j].lru = zero;
      cache->sets[i].lines[j].dirty = false;
    }
  }
  return success;
}


//store helper method that loads and keeps track of dirty bit on top of it
void store(struct Cache cache, mem_addr_t address, int b, int s, int E, int B, unsigned* hits, unsigned* misses, unsigned* eviction, unsigned* dirty_bytes_evicted, unsigned* dirty_bytes_active, unsigned* double_ref ){
  int setIndex = (address >> b) & ((1 << s) - 1);
  int tag = address >> (b + s);
  char fullSet = 1;
  char hit = 0;
  int indexOfEmptyLine = -1;
  for (int i = 0; i < E; i++)
  {
    //hit
    if (cache.sets[setIndex].lines[i].valid && cache.sets[setIndex].lines[i].tag == tag)
    {
      //check for least recently used block
      if(cache.sets[setIndex].lines[i].lru > 0){
        printf("hit ");
      }
      else{
        (*double_ref)++;
        printf("hit-double-ref ");
      }
      hit = 1;
      (*hits)++;
      //check if line is dirty, and update dirty_bytes_active accordingly
      if(!cache.sets[setIndex].lines[i].dirty)
      {
        (*dirty_bytes_active) += B;
      }
      //set line to dirty
      cache.sets[setIndex].lines[i].dirty = true;
      //update lru
      for (int j = 0; j < E; j++)
      {
        if (cache.sets[setIndex].lines[j].valid)
        {
          cache.sets[setIndex].lines[j].lru++;
        }
      }
      cache.sets[setIndex].lines[i].lru = 0;
      break;
    }
  }
  //miss
  if (!hit)
  {
    (*misses)++;
    printf("miss ");
    //check each line for any empty line
    for(int i = 0; i < E; i++){
      if(!cache.sets[setIndex].lines[i].valid){
        fullSet = 0;
        indexOfEmptyLine = i;
        break;
      }
    }
    //if there is an empty line
    if(!fullSet){
      //overwrite data and update dirty_bytes_active
      cache.sets[setIndex].lines[indexOfEmptyLine].valid = true;
      cache.sets[setIndex].lines[indexOfEmptyLine].dirty = true;
      cache.sets[setIndex].lines[indexOfEmptyLine].tag = tag;
      (*dirty_bytes_active) += B;
      //update lru
      for (int i = 0; i < E; ++i)
      {
        if (cache.sets[setIndex].lines[i].valid)
        {
          cache.sets[setIndex].lines[i].lru++;
        }
      }
      cache.sets[setIndex].lines[indexOfEmptyLine].lru = 0;
    }
    //no empty line, so we have to evict
    else{
      (*eviction)++;
      //find the line to evict using lru policy
      int max_lru_offset = 0;
      int max_lru = 0;
      for (int i = 0; i < E; ++i) {
        if (cache.sets[setIndex].lines[i].lru > max_lru) {
          max_lru = cache.sets[setIndex].lines[i].lru;
          max_lru_offset = i;
        }
      }
      cache.sets[setIndex].lines[max_lru_offset].tag = tag;
      //check if the line to be evicted is dirty
      if(cache.sets[setIndex].lines[max_lru_offset].dirty){
        (*dirty_bytes_evicted) += B;
        (*dirty_bytes_active) -= B;
        printf("dirty-eviction ");
      }
      else{
        printf("eviction ");
      }
      (*dirty_bytes_active) += B;
      cache.sets[setIndex].lines[max_lru_offset].dirty = true;
      //update lru
      for (int i = 0; i < E; ++i) {
        if (cache.sets[setIndex].lines[i].valid){
          cache.sets[setIndex].lines[i].lru++;
        }
      }
      cache.sets[setIndex].lines[max_lru_offset].lru = 0;
    }
  }
}


//load helper method that attempts to read data from the cache
void load(struct Cache cache, mem_addr_t address, int b, int s, int E, int B, unsigned* hits, unsigned* misses, unsigned* eviction, unsigned* dirty_bytes_evicted, unsigned* dirty_bytes_active, unsigned* double_ref) {
  int setIndex = (address >> b) & ((1 << s) - 1);
  int tag = address >> (b + s);
  char fullSet = 1;
  char hit = 0;
  int indexOfEmptyLine = -1;
  for (int i = 0; i < E; i++) {
    //hit
    if (cache.sets[setIndex].lines[i].valid && cache.sets[setIndex].lines[i].tag == tag) {
      //check for least recently used block
      if(cache.sets[setIndex].lines[i].lru > 0){
        printf("hit ");
      }
      else{
        (*double_ref)++;
        printf("hit-double-ref ");
      }
      hit = 1;
      (*hits)++;
      //update lru
      for (int j = 0; j < E; j++) {
        if (cache.sets[setIndex].lines[j].valid) {
          cache.sets[setIndex].lines[j].lru++;
        }
      }
      cache.sets[setIndex].lines[i].lru = 0;
      break;
    }
  }
//miss
  if (!hit) {
    (*misses)++;
    printf("miss ");
    //check each line for any empty line
    for(int i = 0; i < E; i++){
      if(!cache.sets[setIndex].lines[i].valid){
        fullSet = 0;
        indexOfEmptyLine = i;
        break;
      }
    }
    //if there is an empty line
    if(!fullSet){
      //overwrite date
      cache.sets[setIndex].lines[indexOfEmptyLine].valid = true;
      cache.sets[setIndex].lines[indexOfEmptyLine].tag = tag;
      cache.sets[setIndex].lines[indexOfEmptyLine].dirty = false;
      //update lru
      for (int i = 0; i < E; ++i) {
        if (cache.sets[setIndex].lines[i].valid) {
          cache.sets[setIndex].lines[i].lru++;
        }
      }
      cache.sets[setIndex].lines[indexOfEmptyLine].lru = 0;
    }
    else{
      //set is full, so we need to evict
      (*eviction)++;
      int max_lru_offset = 0;
      int max_lru = 0;
      //find the line to evict using lru policy
      for (int i = 0; i < E; ++i) {
        if (cache.sets[setIndex].lines[i].lru > max_lru) {
          max_lru = cache.sets[setIndex].lines[i].lru;
          max_lru_offset = i;
        }
      }
      //check if the line is dirty
      cache.sets[setIndex].lines[max_lru_offset].tag = tag;
      if(cache.sets[setIndex].lines[max_lru_offset].dirty){
        (*dirty_bytes_evicted) += B;
        (*dirty_bytes_active) -= B;
        printf("dirty-eviction ");
      }
      else{
        printf("eviction ");
      }
      cache.sets[setIndex].lines[max_lru_offset].dirty = false;
      //update lru
      for (int i = 0; i < E; ++i) {
        if(cache.sets[setIndex].lines[i].valid){
          cache.sets[setIndex].lines[i].lru++;
        }
      }
      cache.sets[setIndex].lines[max_lru_offset].lru = 0;
    }
  }
}

int main(int argc, char **argv)
{
    int option;
  int s = 0;
  int E = 0;
  int b = 0;
  int S = 0;
  int v = 0;
  int B = 0;
  char *tracefile = NULL;
  unsigned hits = 0;
  unsigned misses = 0;
  unsigned eviction = 0;
  unsigned dirty_bytes_evicted = 0;
  unsigned dirty_bytes_active = 0;
  unsigned double_ref = 0;
  //parse through options
  while ((option = getopt(argc, argv, "hvs:E:b:t:")) != invalid)
  {
    switch (option)
    {
    case 'h':
      print_help();
      return success;
    case 'v':
      v = 1;
      break;
    case 's':
      s = atoi(optarg);
      S = pow(2, s);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 'b':
      b = atoi(optarg);
      B = pow(2, b);
      break;
    case 't':
      tracefile = optarg;
      break;
    default:
      printf("Unknown Argument.\n");
      print_help();
      return failure;
    }
  }
if (s == 0 || E == 0 || b == 0 || tracefile == NULL)
  {
    printf("Argument Missing.\n");
    print_help();
    return failure;
  }

  struct Cache cache;

  if(buildCache(S, E, &cache) == failure){
    return failure;
  }
  //parse through tracefile
  FILE *fp = fopen(tracefile, "r");
  char line[lengthOfLine];
  unsigned len = 0;
  mem_addr_t address = 0;
  if (fp == NULL)
  {
    fprintf(stderr, "Failed to open trace file.");
    return failure;
  }
  while (fgets(line, lengthOfLine, fp) != NULL)
  {
    if (line[zero] == 'I')
    {
      continue;
    }

    sscanf(line+toAddressIndex, "%llx,%u", &address, &len);
    if (v == true)
    {
      printf("%c %llx,%u ", line[opIndex], address, len);
    }

    switch (line[1])
    {
      case 'M':
        load(cache, address,  b,  s,  E, B, &hits, &misses, &eviction,  &dirty_bytes_evicted, &dirty_bytes_active, &double_ref);
        store(cache, address,  b,  s,  E, B, &hits, &misses, &eviction, &dirty_bytes_evicted, &dirty_bytes_active, &double_ref);
        break;
      case 'S':
        store(cache, address,  b,  s,  E, B, &hits, &misses, &eviction, &dirty_bytes_evicted, &dirty_bytes_active, &double_ref);
        break;
      case 'L':
        load(cache, address,  b,  s,  E, B, &hits, &misses, &eviction,  &dirty_bytes_evicted, &dirty_bytes_active, &double_ref);
        break;
    }
    printf("\n");
  }


  

  for (int i = 0; i < S; i++) {
      free(cache.sets[i].lines);
  }
  free(cache.sets);

  fclose(fp);

  printSummary(hits, misses, eviction, dirty_bytes_evicted, dirty_bytes_active , double_ref);
  return success;
}
