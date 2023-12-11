#ifndef QUEUE_H
#define QUEUE_H

#define QUEUE_SIZE 11 // CACHE_SIZE + 1

typedef struct queue {
    int front;
    int rear;
    int block_nums[QUEUE_SIZE];
} Queue; // circular queue

Queue *init_queue();
void free_queue(Queue *q);
int is_full(Queue *q);
int is_empty(Queue *q);
void enqueue(Queue *q, int block_nr);
int dequeue(Queue *q);

#endif