#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "buffercache.h"
#include <pthread.h>
#include <sys/time.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

BufferCache *buffer_init() {
    BufferCache *bc = (BufferCache*)malloc(sizeof(BufferCache));;
    bc->items = 0;
    memset(bc->array, 0, sizeof(bc->array));
    Queue *q = init_queue(); // FIFO 용 큐 생성
    bc->cachequeue = q;

    Stack *s = init_stack(); // LRU 용 스택 생성 
    bc->cachstack = s; 

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
    // 시간 측정
    struct timeval start_time, end_time;
    long elapsed_time;
    gettimeofday(&start_time, NULL);

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

            // 찾은 블록의 번호를 스택에서 삭제
            for (int i = 0; i < CACHE_SIZE; i++) {
                if (buffercache->cachstack->items[i] == block_nr) {
                    for (int j = i; j < CACHE_SIZE - 1; j++) {
                        buffercache->cachstack->items[j] = buffercache->cachstack->items[j + 1];
                    }
                    break;
                }
            }

            // 스택의 맨 위로 push
            push(buffercache->cachstack, block_nr);

            gettimeofday(&end_time, NULL);
            elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000L +
            (end_time.tv_usec - start_time.tv_usec);
            printf("[Read hit] %ld microseconds\n", elapsed_time);
            return 0;
        }
        current = current->next;
    }
    gettimeofday(&end_time, NULL);
    elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000L +
    (end_time.tv_usec - start_time.tv_usec);
    printf("[Read miss] %ld microseconds\n", elapsed_time);
    return -1;
}

// 스레드가 실행할 함수
void *direct_io(void *ptr) {
    pthread_mutex_lock(&lock); // lock
    int disk_fd = open("diskfile", O_RDWR|O_DIRECT);

    Args *args = (Args *)ptr;
    int block_nr = args->victim_block_nr;
    char *data = args->data;
    if (lseek(disk_fd, block_nr * BLOCK_SIZE, SEEK_SET) < 0)
        perror("disk is not mounted");
    if (write(disk_fd, data, BLOCK_SIZE) < 0)
        perror("write - direct_io");
    close(disk_fd);
    pthread_mutex_unlock(&lock); // unlock
}

// function for flush thread (every 15 secs)
void *flush(void *ptr) {
    BufferCache *bc = (BufferCache *) ptr;

    while (1) {
        printf("flushing...\n");
        for (int i=0; i< CACHE_SIZE; i++) {
            Node *current = bc->array[i];
            while (current != NULL) {
                if (current->blk->dirty_bit == 1) {
                    pthread_mutex_lock(&lock); // lock
                    int block_nr = current->blk->block_nr;
                    char *data = current->blk->data;
                    int disk_fd = open("diskfile", O_RDWR|O_DIRECT);
                    if (lseek(disk_fd, block_nr * BLOCK_SIZE, SEEK_SET) < 0)
                        perror("disk is not mounted");
                    if (write(disk_fd, data, BLOCK_SIZE) < 0)
                        perror("write - flush");
                    pthread_mutex_unlock(&lock); // unlock
                }
                current = current->next;
            }
        }
        sleep(15);
    }
}

/* mode (버퍼캐시가 꽉 찼을때 한정) :: victim 선정 알고리즘
0 : FIFO
1 : LRU
2 : LFU */
int delayed_write(BufferCache *buffercache, int block_nr, char *input, int mode) {

    if (block_nr < 0 || block_nr > DISK_BLOCKS-1) {
        return -1;
    }

    pthread_t thread;

    int victim_block_nr;
    // 버퍼캐시가 꽉참 -> victim 선정
    if (buffercache->items == CACHE_SIZE) {
        char *data = (char *)malloc(BLOCK_SIZE);
        switch (mode) {
            case 0:
                victim_block_nr = fifo(buffercache, data);
                printf("victim : %s\n", data);
                break;
            case 1:
              victim_block_nr = lru(buffercache, data);
                printf("victim : %s\n", data);
                break;
                break;
            case 2:
                victim_block_nr = lfu(buffercache, data);
                printf("victim : %s\n", data);
                break;
                break;
            default:
                return -1;
        }
        // 스레드 생성 -> direct_io
        Args *a = (Args *)malloc(sizeof(Args));
        a->victim_block_nr = victim_block_nr;
        strcpy(a->data, data);
        int tid = pthread_create(&thread, NULL, direct_io, (void *)a);
        pthread_join(thread, NULL);
        free(data);
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
    push(buffercache->cachstack,block_nr); // block_nr을 스택에 집엉넣음

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

int lru(BufferCache *buffercache, char *placeholder) {
    int oldest_block_nr = -1;
    int oldest_index = -1;

    for (int i = 0; i < CACHE_SIZE; i++) {
        int block_nr = buffercache->cachstack->items[i];
        int index = hash(block_nr);
        Node *current = buffercache->array[index];
        while (current != NULL) {
            if (current->blk->block_nr == block_nr) {
                if (oldest_block_nr == -1) {
                    oldest_block_nr = current->blk->block_nr;
                    oldest_index = index;
                }
                break;
            }
            current = current->next;
        }
    }
    printf("[LRU] block_num %d will be deleted\n", oldest_block_nr);

    // 찾은 오래된 블록을 삭제하고, 데이터를 placeholder에 복사
    if (oldest_block_nr != -1) {
        int index = oldest_index;
        Node *current = buffercache->array[index];
        Node *prev = NULL;

        while (current != NULL) {
            if (current->blk->block_nr == oldest_block_nr) {
                if (prev == NULL) {
                    buffercache->array[index] = current->next;
                } else {
                    prev->next = current->next;
                }

                strcpy(placeholder, current->blk->data);
                free(current->blk);
                free(current);
                buffercache->items--;

                dequeue(buffercache->cachequeue);
                 // 스택에서도 제거하고 가장 위로 올림
                for (int i = 0; i < CACHE_SIZE - 1; i++) {
                    buffercache->cachstack->items[i] = buffercache->cachstack->items[i + 1];
                }
                buffercache->cachstack->top--;
                // buffercache->cachstack->items[CACHE_SIZE - 1] = oldest_block_nr;
                // 스택에서도 제거
                // for (int i = 0; i < CACHE_SIZE - 1; i++) {
                //     buffercache->cachstack->items[i] = buffercache->cachstack->items[i + 1];
                // }
                buffercache->cachstack->top--;

                return oldest_block_nr;
            }

            prev = current;
            current = current->next;
        }
    }
    perror("Entry Not found");
    return -1;
}

int lfu(BufferCache *buffercache, char *placeholder) {
    int min_ref_count = 10000;
    int victim_block_nr = -1;

    for (int i = 0; i < CACHE_SIZE; i++) {
        Node *current = buffercache->array[i];
        while (current != NULL) {
            if (current->blk->ref_count < min_ref_count) {
                min_ref_count = current->blk->ref_count;
                victim_block_nr = current->blk->block_nr;
            }
            current = current->next;
        }
    }

    // Check if victim found
    if (victim_block_nr != -1) {
        int index = hash(victim_block_nr);
        Node *current = buffercache->array[index];
        Node *prev = NULL;

        // Remove victim block from array
        while (current != NULL) {
            if (current->blk->block_nr == victim_block_nr) {
                if (prev == NULL) {
                    buffercache->array[index] = current->next;
                } else {
                    prev->next = current->next;
                }
                current->blk->ref_count = 0;
                strcpy(placeholder, current->blk->data);
                free(current->blk);
                free(current);
                buffercache->items--;

                // queue, stack, ref_count 초기화 
                pop(buffercache->cachstack);
                dequeue(buffercache->cachequeue);

                return victim_block_nr; // Return victim block number
            }

            prev = current;
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

    pthread_t flush_thread;

    int tid = pthread_create(&flush_thread, NULL, flush, (void *)bc);
    pthread_detach(flush_thread);

    // direct_IO
    char *test_buf = (char *)malloc(BLOCK_SIZE);

    int ret = buffered_read(bc, 0, output);

    printf("[READ] block_num 0 : %s\n", output); // What color is grass?

    ret = delayed_write(bc, 11, "White", 1);

    ret = buffered_read(bc, 11, output);

    int disk_fd = open("diskfile", O_RDWR|O_DIRECT);
    if (lseek(disk_fd, 0 * BLOCK_SIZE, SEEK_SET) < 0)
        perror("lseek");
    if (read(disk_fd, test_buf, BLOCK_SIZE) < 0)
        perror("read");
    printf("[DIRECT I/O block_num 0 (after FIFO) : %s\n", test_buf);

    if (read(disk_fd, test_buf, BLOCK_SIZE) < 0)
        perror("read");

    printf("[READ] block_num 11 : %s\n", output);

    ret = delayed_write(bc, 19, "Black", 1);

    ret = buffered_read(bc, 19, output);

    printf("[READ] block_num 19 : %s\n", output);

    buffer_free(bc);
    
    return 0;
}