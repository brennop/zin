#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

/**
 * Inicializa uma fila queue com capacidade capcity
 */
void queue_init(queue_t *queue, int capacity);

/**
 * tenta inserir um elemento na fila.
 * retorna -1 se não for possível e.g. a fila está cheia.
 */
int queue_trypush(queue_t *queue, void *element);

/**
 * Tenta inserir um elemento na fila.
 * Bloqueia se a fila estiver cheia
 */
int queue_push(queue_t *queue, void *element);

/**
 * Tenta remover uma elemento da fila.
 * Bloqueia a execução da thread se a fila estiver vazia.
 */
void queue_pop(queue_t *queue, void **element);

/**
 * Obtém o elemento em index na fila.
 * Bloqueia a execução da thread se a fila estiver vazia.
 * Se ficar bloqueado por mais de timeout_sec, retorna -1.
 */
int queue_get(queue_t *queue, void **element, int *index, int timeout_sec);

void queue_out(queue_t *queue, int *out);

void queue_destroy(queue_t *queue);
