#include <pthread.h>
#include <stdlib.h>

#define MAX_SUBSCRIPTIONS 128

typedef struct {
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
  pthread_mutex_t mutex;
  /** condicional para esperar a fila ter elementos */
  pthread_cond_t not_empty;
  /** condicional para esperar a fila ter elementos */
  pthread_cond_t not_full;
} queue_t;

void *stream_replicate(void *stream);

typedef struct {
  int subscribers;
  queue_t publisher_queue;
  queue_t *subscriber_queues[MAX_SUBSCRIPTIONS];

  pthread_t thread;
  pthread_mutex_t mutex;
} stream_t;

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
    queue->data[queue->in] = element;
    queue->in++;

    if (queue->in >= queue->bounds) {
      queue->in -= queue->bounds;
    }

    queue->length++;

    pthread_cond_signal(&queue->not_empty);
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

    queue->data[queue->in] = element;
    queue->in++;

    if (queue->in >= queue->bounds) {
      queue->in -= queue->bounds;
    }

    queue->length++;

    pthread_cond_signal(&queue->not_empty);
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

    *element = queue->data[queue->out];
    queue->length--;

    queue->out++;
    if (queue->out >= queue->bounds) {
      queue->out -= queue->bounds;
    }
  }
  pthread_mutex_unlock(&queue->mutex);
}

void queue_destroy(queue_t *queue) {
  free(queue->data);
}

void stream_init(stream_t *stream) {
  stream->subscribers = 0;

  queue_init(&stream->publisher_queue, 24);

  pthread_mutex_init(&stream->mutex, 0);

  pthread_create(&stream->thread, NULL, stream_replicate, stream);
}

int stream_subscribe(stream_t *stream, queue_t *subcription_queue) {
  int subscription;
  pthread_mutex_lock(&stream->mutex);
  {
    // TODO: lançar um erro
    if (stream->subscribers >= MAX_SUBSCRIPTIONS) {
      return -1;
    }

    for (subscription = 0; subscription < MAX_SUBSCRIPTIONS; subscription++) {
      if (stream->subscriber_queues[subscription] == NULL)
        break;
    }

    stream->subscriber_queues[subscription] = subcription_queue;
    stream->subscribers++;
  }
  pthread_mutex_unlock(&stream->mutex);
  return subscription;
}

void stream_unsubscribe(stream_t *stream, int subscription) {
  if (subscription < 0 || subscription > MAX_SUBSCRIPTIONS) {
    return;
  }

  pthread_mutex_lock(&stream->mutex);
  {
    stream->subscriber_queues[subscription] = NULL;
    stream->subscribers--;
  }
  pthread_mutex_unlock(&stream->mutex);
}
