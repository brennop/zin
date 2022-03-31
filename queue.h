#include <stdlib.h>

typedef struct Queue {
  void **data;
  int length;
  int cursor;
  int size;
} Queue;

Queue *newQueue(int size) {
  Queue *queue = (Queue*) malloc(sizeof(*queue));
  queue->size = size;
  queue->cursor = 0;
  queue->length = 0;
  queue->data = (void**) calloc(sizeof(*queue->data), size);
  return queue;
}

void push(Queue *queue, void *data) { queue->data[queue->length++ % queue->size] = data; }

void *dequeue(Queue *queue) { return queue->data[queue->cursor++ % queue->size]; }
