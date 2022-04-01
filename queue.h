#include <pthread.h>
#include <stdlib.h>

typedef struct queue_t {
  /** array de elementos */
  void **data;
  /** indíce para inserir */
  int in;
  /** indíce para remover */
  int out;
  /** tamanho da fila */
  int bounds;
  /** qtd de elementos na fila */
  int length;

  pthread_mutex_t *lock;
  pthread_cond_t *waiting;
} queue_t;

void queue_init(queue_t *queue, int capacity) {
  queue->bounds = capacity;
  queue->data = (void **)calloc(capacity, sizeof(*queue->data));
  queue->length = 0;
  queue->in = 0;
  queue->out = 0;

  queue->lock = (pthread_mutex_t *)malloc(sizeof(*queue->lock));
  queue->waiting = (pthread_cond_t *)malloc(sizeof(*queue->waiting));

  pthread_mutex_init(queue->lock, 0);
  pthread_cond_init(queue->waiting, 0);
}

int queue_push(queue_t *queue, void *element) {
  pthread_mutex_lock(queue->lock);
  {
    // se a fila estiver cheia
    if (queue->length == queue->bounds) {
      pthread_mutex_unlock(queue->lock);
      return -1;
    }
    queue->data[queue->in] = element;
    queue->in++;

    if (queue->in >= queue->bounds) {
      queue->in -= queue->bounds;
    }

    queue->length++;

    pthread_cond_signal(queue->waiting);
  }
  pthread_mutex_unlock(queue->lock);
  return 0;
}

void queue_pop(queue_t *queue, void **element) {
  pthread_mutex_lock(queue->lock);
  {
    while (queue->length == 0) {
      pthread_cond_wait(queue->waiting, queue->lock);
    }

    *element = queue->data[queue->out];
    queue->length--;

    queue->out++;
    if (queue->out >= queue->bounds) {
      queue->out -= queue->bounds;
    }
  }
  pthread_mutex_unlock(queue->lock);
}
