#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <getopt.h>
#include "cachelab.h"

typedef unsigned long long int mem_addr_t;

struct Trace
{
  char type;
  long addr;
  int size;
};

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

enum
{
  HIT,
  MISS,
  EVICTION
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
    }
    for (int j = 0; j < E; ++j)
    {
      cache->sets[i].lines[j].valid = false;
      cache->sets[i].lines[j].tag = 0;
      cache->sets[i].lines[j].lru = 0;
      cache->sets[i].lines[j].dirty = false;
    }
  }
  return success;
}

void store(struct Cache cache, mem_addr_t address, int b, int s, int E, unsigned* hits, unsigned* misses, unsigned* eviction, unsigned* dirty_bytes_active, unsigned* dirty_bytes_evicted ){
  int setIndex = (address >> b) & ((1 << s) - 1);
  int tag = address >> (b + s);
  for(int dk = 0; dk < 1; dk++)
  {
    char hit = 0;
    int indexOfEmptyLine = -1;
    for (int i = 0; i < E; i++)
    {
      if (cache.sets[setIndex].lines[i].valid && cache.sets[setIndex].lines[i].tag == tag)
      {
        printf("hits ");
        hit = 1;
        (*hits)++;
        if(cache.sets[setIndex].lines[i].dirty)
        {
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
        else{
          dirty_bytes_active++;
          cache.sets[setIndex].lines[i].dirty = true;
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
    }
    if (!hit)
    {
      (*misses)++;
      printf("misses ");
      for(indexOfEmptyLine = 0; indexOfEmptyLine < E; indexOfEmptyLine++)
      {
        if (cache.sets[setIndex].lines[indexOfEmptyLine].valid)
        {
          if(cache.sets[setIndex].lines[indexOfEmptyLine].tag != tag){
            (*eviction)++;
            
            if(cache.sets[setIndex].lines[indexOfEmptyLine].dirty){
              for(int i = 0; i < E; i++){
                (*dirty_bytes_evicted)++;
              }
              printf("dirty eviction ");
            }
            else{
              printf("eviction ");
            }
            int max_lru_offset = 0;
            int max_lru = 0;
            for (int i = 0; i < E; ++i)
            {
              if (cache.sets[setIndex].lines[i].lru > max_lru)
              {
                max_lru = cache.sets[setIndex].lines[i].lru;
                max_lru_offset = i;
              }
            }
            cache.sets[setIndex].lines[max_lru_offset].tag = tag;
            for (int i = 0; i < E; ++i)
            {
              cache.sets[setIndex].lines[i].lru++;
            }
            cache.sets[setIndex].lines[max_lru_offset].lru = 0;
          }
          break;
        }
        else
        {
          cache.sets[setIndex].lines[indexOfEmptyLine].valid = true;
          cache.sets[setIndex].lines[indexOfEmptyLine].dirty = true;
          dirty_bytes_active++;
          cache.sets[setIndex].lines[indexOfEmptyLine].tag = tag;
          for (int i = 0; i < E; ++i)
          {
            if (cache.sets[setIndex].lines[i].valid)
            {
              cache.sets[setIndex].lines[i].lru++;
            }
          }
          cache.sets[setIndex].lines[indexOfEmptyLine].lru = 0;

        }
      }
    }
  }
}

void load(struct Cache cache, mem_addr_t address, int b, int s, int E, unsigned* hits, unsigned* misses, unsigned* eviction, unsigned* dirty_bytes_active) {
  int setIndex = (address >> b) & ((1 << s) - 1);
  int tag = address >> (b + s);
  char fullSet = 1;
  char hit = 0;
  int indexOfEmptyLine = -1;
  for (int i = 0; i < E; i++) {
    if (cache.sets[setIndex].lines[i].valid && cache.sets[setIndex].lines[i].tag == tag) {
      printf("hits ");
      hit = 1;
      (*hits)++;
      for (int j = 0; j < E; j++) {
        if (cache.sets[setIndex].lines[j].valid) {
          cache.sets[setIndex].lines[j].lru++;
        }
      }
      cache.sets[setIndex].lines[i].lru = 0;
      break;
    } 
  }
  if (!hit) {
    (*misses)++;
    printf("misses ");
    for(indexOfEmptyLine = 0; indexOfEmptyLine < E; indexOfEmptyLine++){
      if (cache.sets[setIndex].lines[indexOfEmptyLine].valid) {
        if (cache.sets[setIndex].lines[indexOfEmptyLine].tag != tag) {
          (*eviction)++;
          if(cache.sets[setIndex].lines[indexOfEmptyLine].dirty){
            (*dirty_bytes_active)++;
            printf("dirty eviction ");
          }
          else{
            printf("eviction ");
          }
          int max_lru_offset = 0;
          int max_lru = 0;
          for (int i = 0; i < E; ++i) {
            if (cache.sets[setIndex].lines[i].lru > max_lru) {
              max_lru = cache.sets[setIndex].lines[i].lru;
              max_lru_offset = i;
            }
          }
          cache.sets[setIndex].lines[max_lru_offset].tag = tag;
          for (int i = 0; i < E; ++i) {
            cache.sets[setIndex].lines[i].lru++;
          }
          cache.sets[setIndex].lines[max_lru_offset].lru = 0;
        }
        break;
      } else {
        cache.sets[setIndex].lines[indexOfEmptyLine].valid = true;
        cache.sets[setIndex].lines[indexOfEmptyLine].tag = tag;
        for (int i = 0; i < E; ++i) {
          if (cache.sets[setIndex].lines[i].valid) {
            cache.sets[setIndex].lines[i].lru++;
          }
        }
        cache.sets[setIndex].lines[indexOfEmptyLine].lru = 0;
      }
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
  char *tracefile = NULL;
  unsigned hits = 0;
  unsigned misses = 0;
  unsigned eviction = 0;
  unsigned dirty_bytes_evicted = 0;
  unsigned dirty_bytes_active = 0;

  while ((option = getopt(argc, argv, "hvs:E:b:t:")) != -1)
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

  buildCache(S, E, &cache);

  FILE *fp = fopen(tracefile, "r");
  char line[100];
  unsigned len = 0;
  mem_addr_t address = 0;
  if (fp == NULL)
  {
    fprintf(stderr, "Failed to open trace file.");
    return failure;
  }

  while (fgets(line, 100, fp) != NULL)
  {
    if (line[0] == 'I')
    {
      continue;
    }

    sscanf(line+3, "%llx,%u", &address, &len);
    if (v == 1)
    {
      printf("%c %llx,%u ", line[1], address, len);
    }
    switch (line[1])
    {
      case 'M':
        load(cache, address,  b,  s,  E, &hits, &misses, &eviction,  &dirty_bytes_active);
        store(cache, address,  b,  s,  E, &hits, &misses, &eviction, &dirty_bytes_evicted, &dirty_bytes_active);
        break;
      case 'S':
        store(cache, address,  b,  s,  E, &hits, &misses, &eviction, &dirty_bytes_evicted, &dirty_bytes_active);
        break;
      case 'L':
        load(cache, address,  b,  s,  E, &hits, &misses, &eviction, &dirty_bytes_active);
        break;
    }
    printf("\n");
  }

  

  for (int i = 0; i < S; i++) {
      free(cache.sets[i].lines);
  }
  free(cache.sets);

  fclose(fp);

  printSummary(hits, misses, eviction, dirty_bytes_evicted, dirty_bytes_active ,0);
  return success;
}

        // (*dirty_bytes_evicted) += B;
        // (*dirty_bytes_active) -= B;