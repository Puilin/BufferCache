#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "buffercache.h"
#include <pthread.h>

BufferCache *buffer_init() {
    BufferCache *bc = (BufferCache*)malloc(sizeof(BufferCache));;
    bc->items = 0;
    memset(bc->array, 0, sizeof(bc->array));
    Queue *q = init_queue(); // FIFO 용 큐 생성
    bc->cachequeue = q;
    return bc;
}

void buffer_free(BufferCache *bc) {
    // 각 노드에 대한 메모리 해제
    for (int i = 0; i < CACHE_SIZE; i++) {
        Node *current = bc->array[i];
        while (current != NULL) {
            if (current->blk != NULL)
                free(current->blk);
            Node *next = current->next;
            free(current);
            current = next;
        }
    }

    free_queue(bc->cachequeue);
    // BufferCache 구조체에 대한 메모리 해제
    free(bc);
}

/* 해시함수 */
int hash(int input) {
    return input % CACHE_SIZE;
}

int buffered_read(BufferCache *buffercache, int block_nr, char *result) {
    if (block_nr < 0 || block_nr > DISK_BLOCKS-1) {
        return -1;
    }
    int index = hash(block_nr);
    Node *current = buffercache->array[index];
    while (current != NULL) {
        // found
        if (current->blk->block_nr == block_nr) {
            memcpy(result, current->blk->data, BLOCK_SIZE);
            current->blk->ref_count++;
            return 0;
        }
        current = current->next;
    }
    return -1;
}

// 스레드가 실행할 함수
void *direct_io(void *ptr) {
    
}

/* mode (버퍼캐시가 꽉 찼을때 한정) :: victim 선정 알고리즘
0 : FIFO
1 : LRU
2 : LFU */
int delayed_write(BufferCache *buffercache, int block_nr, char *input, int mode) {

    if (block_nr < 0 || block_nr > DISK_BLOCKS-1) {
        return -1;
    }
    
    char *data = (char *)malloc(BLOCK_SIZE);
    int victim_block_nr;
    // 버퍼캐시가 꽉참 -> victim 선정
    if (buffercache->items == CACHE_SIZE) {
        switch (mode) {
            case 0:
                victim_block_nr = fifo(buffercache, data);
                printf("victim : %s\n", data);
                break;
            case 1:
                //lru();
                break;
            case 2:
                //lfu();
                break;
            default:
                return -1;
        }
        // 스레드 생성 -> direct_io
        //return -1;
    }

    // buffer cache에 쓰기

    struct block *blk = (struct block*)malloc(sizeof(struct block));
    blk->block_nr = block_nr;
    strcpy(blk->data, input);
    blk->dirty_bit = 1;
    blk->ref_count = 0;

    Node *node = (Node *)malloc(sizeof(Node));
    node->blk = blk;
    node->next = NULL;

    int index = hash(block_nr);
    Node *current = buffercache->array[index];
    if (current == NULL) {
        buffercache->array[index] = node;
    } else { // not NULL
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = node;
    }
    buffercache->items++;
    enqueue(buffercache->cachequeue, block_nr); // block_nr를 큐에 집어넣음
    
    return 0;
}

int fifo(BufferCache *bc, char *placeholder) {
    Queue *q = bc->cachequeue;
    int block_nr = dequeue(q);
    int index = hash(block_nr);

    char *victim = (char *)malloc(BLOCK_SIZE);

    printf("[FIFO] block_num %d will be deleted\n", block_nr);

    Node *current = bc->array[index];
    // victim이 첫번째 노드인 경우
    if (current->blk->block_nr == block_nr) {
        strcpy(placeholder, current->blk->data);
        free(current->blk);
        bc->array[index] = NULL;
        bc->items--;
        return block_nr;
    } else {
        while (current->next != NULL) {
            // found
            if (current->next->blk->block_nr == block_nr) {
                strcpy(placeholder, current->blk->data);
                free(current->next->blk);
                current->next = NULL;
                bc->items--;
                return block_nr;
            }
            current = current->next;
        }
    }
    perror("Entry Not found");
    return -1;
}

int main() {
    char testbank[10][BLOCK_SIZE] = {"What color is grass?","Green","Red","Pink","Yellow","Purple","Orange","Violet","Brown","Gold"};

    char *output = (char *)malloc(BLOCK_SIZE);

    BufferCache *bc = buffer_init();

    for (int i=0; i<10; i++) {
        delayed_write(bc, i, testbank[i], 0);
    }

    int ret = buffered_read(bc, 9, output);

    printf("[READ] block_num 9 : %s\n", output); // Gold

    ret = delayed_write(bc, 11, "White", 0);

    ret = buffered_read(bc, 11, output);

    printf("[READ] block_num 11 : %s\n", output);

    ret = delayed_write(bc, 19, "Black", 0);

    ret = buffered_read(bc, 19, output);

    printf("[READ] block_num 19 : %s\n", output);

    buffer_free(bc);
    
    return 0;
}