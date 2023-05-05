#include "cachelab.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>

int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;
int verbose = 0;
char t[100];

void printUsage()
{
    printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <s>     Number of set index bits.\n");
    printf("  -E <E>     Number of lines per set.\n");
    printf("  -b <b>     Number of block offset bits.\n");
    printf("  -t <tracefile>  Name of the valgrind trace to replay.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
    fflush(stdout);
}

int s = 0;
int E = 0;
int b = 0;
typedef struct
{
    int valid;
    int tag;
    int lru;
} cache_line;
cache_line **cache = NULL;

void initCache()
{
    int S = 1 << s;
    cache = (cache_line **)malloc(sizeof(cache_line *) * S);
    for (int i = 0; i < S; i++)
    {
        cache[i] = (cache_line *)malloc(sizeof(cache_line) * E);
        for (int j = 0; j < E; j++)
        {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].lru = 0;
        }
    }
}

//visit a memory address
void visit(int set, int tag, char op, unsigned int addr, int size)
{
    //hit
    for (int i = 0; i < E; i++)
    {
        if (cache[set][i].valid == 1 && cache[set][i].tag == tag)
        {
            cache[set][i].lru = 0;
            hit_count++;
            if (verbose)
                printf("%c %x,%d hit", op, addr, size);
            return;
        }
    }
    // miss
    miss_count++;
    if (verbose)
        printf("%c %x,%d miss", op, addr, size);
    for (int i = 0; i < E; i++)
    {
        if (cache[set][i].valid == 0)
        {
            cache[set][i].valid = 1;
            cache[set][i].tag = tag;
            cache[set][i].lru = 0;
            return;
        }
    }
    // eviction
    eviction_count++;
    if (verbose)
        printf("%c %x,%d eviction", op, addr, size);
    int max = INT_MIN;
    int max_index = 0;
    for (int i = 0; i < E; i++)
    {
        if (cache[set][i].lru > max)
        {
            max = cache[set][i].lru;
            max_index = i;
        }
    }
    cache[set][max_index].tag = tag;
    cache[set][max_index].lru = 0;
    return;
}

void readFile()
{
    FILE *fp = fopen(t, "r");
    if (fp == NULL)
    {
        printf("open file error");
        exit(-1);
    }
    char op;
    unsigned int addr;
    int size;
    while (fscanf(fp, " %c %x,%d", &op, &addr, &size) > 0)
    {
        if (op == 'I')
            continue;
        int S = 1 << s;
        int set = (addr >> b) & (S - 1);
        int tag = addr >> (s + b);
        // update lru
        for (int i = 0; i < E; i++)
        {
            if (cache[set][i].valid == 1)
            {
                cache[set][i].lru++;
            }
        }
        switch (op)
        {
        case 'L':
        case 'S':
            visit(set, tag, op, addr, size);
            break;
        case 'M':
            visit(set, tag, op, addr, size);
            visit(set, tag, op, addr, size);
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    int opt;
    if (argc < 1)
    {
        printUsage();
        return -1;
    }
    // the programme must have 3 arguments：-s, -E, -b
    while (-1 != (opt = (getopt(argc, argv, "hvs:E:b:t:"))))
    {
        switch (opt)
        {
        case 'h':
            printUsage();
            exit(0);
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            // printUsage();
            break;
        }
    }
    if (s <= 0 || E <= 0 || b <= 0 || t == NULL)
    {
        printUsage();
        return -1;
    }
    initCache();
    readFile();
    free(cache);
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
