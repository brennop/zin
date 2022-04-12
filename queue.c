#include "queue.h"

/**
 * Inicializa uma fila queue com capacidade capcity
 */
void queue_init(queue_t *queue, int capacity) {
  queue->bounds = capacity;
  queue->data = (void **)calloc(capacity, sizeof(*queue->data));
  queue->length = 0;
  queue->in = 0;
  queue->out = 0;

  pthread_mutex_init(&queue->mutex, 0);
  pthread_cond_init(&queue->not_empty, 0);
  pthread_cond_init(&queue->not_full, 0);
}

/**
 * tenta inserir um elemento na fila.
 * retorna -1 se não for possível e.g. a fila está cheia.
 */
int queue_trypush(queue_t *queue, void *element) {
  pthread_mutex_lock(&queue->mutex);
  {
    // se a fila estiver cheia
    if (queue->length == queue->bounds) {
      pthread_mutex_unlock(&queue->mutex);
      return -1;
    }
    queue->data[queue->in % queue->bounds] = element;
    queue->in++;

    queue->length++;

    pthread_cond_broadcast(&queue->not_empty);
  }
  pthread_mutex_unlock(&queue->mutex);
  return 0;
}

/**
 * Tenta inserir um elemento na fila.
 * Bloqueia se a fila estiver cheia
 */
int queue_push(queue_t *queue, void *element) {
  pthread_mutex_lock(&queue->mutex);
  {
    // se a fila estiver cheia
    while (queue->length == queue->bounds) {
      // espera a fila abrir um espaço
      pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    queue->data[queue->in % queue->bounds] = element;
    queue->in++;

    queue->length++;

    pthread_cond_broadcast(&queue->not_empty);
  }
  pthread_mutex_unlock(&queue->mutex);
  return 0;
}

/**
 * Tenta remover uma elemento da fila.
 * Bloqueia a execução da thread se a fila estiver vazia.
 */
void queue_pop(queue_t *queue, void **element) {
  pthread_mutex_lock(&queue->mutex);
  {
    while (queue->length == 0) {
      pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    *element = queue->data[queue->out % queue->bounds];
    queue->length--;

    queue->out++;
  }
  pthread_mutex_unlock(&queue->mutex);
}

void queue_get(queue_t *queue, void **element, int *index) {
  pthread_mutex_lock(&queue->mutex);
  {
    while (queue->length == 0 || *index < queue->out || *index >= queue->in) {
      pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    *element = queue->data[*index % queue->bounds];

    *index += 1;
  }
  pthread_mutex_unlock(&queue->mutex);
}

void queue_out(queue_t *queue, int *out) {
  pthread_mutex_lock(&queue->mutex);
  *out = queue->out;
  pthread_mutex_unlock(&queue->mutex);
}

void queue_destroy(queue_t *queue) { free(queue->data); }
