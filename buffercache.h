#ifndef BUFFERCACHE_H
#define BUFFERCACHE_H

#include "queue.h"
#include <time.h>

#define DISK_BLOCKS 100 // also known as N
#define BLOCK_SIZE 4096
#define CACHE_SIZE 10

struct block {
    int block_nr;
    char data[BLOCK_SIZE];
    int dirty_bit; // 1 disk에 쓰이지 않음 0 쓰임
    int ref_count; // 참조횟수
};

typedef struct node {
    struct block *blk;
    struct node *next;
} Node;

typedef struct buffercache {
    Node *array[CACHE_SIZE];
    int items; // node의 개수 총합
    Queue *cachequeue; // FIFO에서 사용됨
    // Stack *cachstack; // LRU에서 사용됨
} BufferCache;

typedef struct thread_args {
    int victim_block_nr;
    char data[BLOCK_SIZE];
} Args;

BufferCache *buffer_init();
void buffer_free(BufferCache *bc);
int hash(int input);
int buffered_read(BufferCache *buffercache, int block_nr, char *result);
void *direct_io(void *ptr);
int delayed_write(BufferCache *buffercache, int block_nr, char *input, int mode);
int fifo(BufferCache *bc, char *data);

#endif