#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"

/* Stack 관련 */
Stack* init_stack() {
    Stack* s = (Stack*)malloc(sizeof(Stack));
    for (int i = 0; i < STACK_SIZE; i++) {
        s->items[i] = 0;
    }
    s->top = -1;
    return s;
}

void free_stack(Stack* s) {
    free(s);
}

void push(Stack* s, int block_nr) {
    if (s->top < STACK_SIZE - 1) {
        s->items[++(s->top)] = block_nr;
    }
}

int pop(Stack* s) {
    if (s->top >= 0) {
        return s->items[(s->top)--];
    }
    return -1; // Stack is empty
}