#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

Queue *init_queue() {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    for (int i = 0; i < QUEUE_SIZE; i++) {
        q->block_nums[i] = 0;
    }
    q->front = 0;
    q->rear = 0;
    return q;
}

void free_queue(Queue *q) {
    free(q);
}

int is_full(Queue *q) {
    return (q->rear+1)%(QUEUE_SIZE)==q->front; // front값이 rear값보다 하나 앞서는 경우
}

int is_empty(Queue *q)  {
    return q->rear==(q->front); //front값과 rear값이 같은 경우
}

void enqueue(Queue *q, int block_nr) {
    if(is_full(q)){
        perror("Queue is Full");
        return ;
    }
    q->rear=(q->rear+1)%(QUEUE_SIZE);
    q->block_nums[q->rear]=block_nr;
}

int dequeue(Queue *q) {
    if(is_empty(q)){
        perror("Queue is Empty");
    }
    q->front++;
    return q->block_nums[(q->front)&(QUEUE_SIZE)];
}