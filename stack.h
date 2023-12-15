#ifndef STACK_H
#define STACK_H

#define STACK_SIZE 11 // CACHE_SIZE + 1

typedef struct stack {
    int top;
    int items[STACK_SIZE];
} Stack;

Stack* init_stack();
void free_stack(Stack* s);
void push(Stack* s, int block_nr);
int pop(Stack* s);

#endif