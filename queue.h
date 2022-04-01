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

  /** mutex para acessar os dados da fila */
  pthread_mutex_t *mutex;
  /** condicional para esperar a fila ter elementos */
  pthread_cond_t *waiting;
} queue_t;

/**
 * Inicializa uma fila queue com capacidade capcity
 */
void queue_init(queue_t *queue, int capacity) {
  queue->bounds = capacity;
  queue->data = (void **)calloc(capacity, sizeof(*queue->data));
  queue->length = 0;
  queue->in = 0;
  queue->out = 0;

  queue->mutex = (pthread_mutex_t *)malloc(sizeof(*queue->mutex));
  queue->waiting = (pthread_cond_t *)malloc(sizeof(*queue->waiting));

  pthread_mutex_init(queue->mutex, 0);
  pthread_cond_init(queue->waiting, 0);
}

/**
 * Tenta inserir um elemento na fila.
 * Retorna -1 se não for possível e.g. a fila está cheia.
 */
int queue_trypush(queue_t *queue, void *element) {
  pthread_mutex_lock(queue->mutex);
  {
    // se a fila estiver cheia
    if (queue->length == queue->bounds) {
      pthread_mutex_unlock(queue->mutex);
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
  pthread_mutex_unlock(queue->mutex);
  return 0;
}

/**
 * Tenta remover uma elemento da fila.
 * Bloqueia a execução da thread se a fila estiver vazia.
 */
void queue_pop(queue_t *queue, void **element) {
  pthread_mutex_lock(queue->mutex);
  {
    while (queue->length == 0) {
      pthread_cond_wait(queue->waiting, queue->mutex);
    }

    *element = queue->data[queue->out];
    queue->length--;

    queue->out++;
    if (queue->out >= queue->bounds) {
      queue->out -= queue->bounds;
    }
  }
  pthread_mutex_unlock(queue->mutex);
}
