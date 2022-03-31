#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <pthread.h>
#include <semaphore.h>

#include "queue.h"

#define PORT 8080
#define MAXLINE 1024
#define MAX_CLIENTS 100

#define POOL_SIZE 20
#define QUEUE_SIZE 1000

pthread_t thread_pool[POOL_SIZE];
Queue *connection_queue;

sem_t queue_empty;
sem_t queue_full;
pthread_mutex_t queue_lock;

void handle_connection(int connection_fd);
char *ROOT;

void check(int value, char *message) {
  if (value == -1) {
    printf("%s\n", message);
    exit(-1);
  }
}

void *thread_handler();

int main() {
  int listen_fd, connection_fd;
  struct sockaddr_in addr;

  /** diretório atual */
  ROOT = getenv("PWD");

  /** cria um socket */
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  check(listen_fd, "Erro ao criar o socket");

  /** inicializa informações de endereçamento */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /** endereço para escutar (any) */
  addr.sin_port = htons(PORT);              /** porta para escutar (8000) */

  /** associa a porta e começa a ouvir nela */
  check(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)),
        "Erro ao associar a porta.");
  check(listen(listen_fd, MAX_CLIENTS), "Erro ao tentar ouvir na porta");
  printf("Esperando conexoes na porta %d\n", PORT);

  /** cria as threads na thread pool */
  for (int i = 0; i < POOL_SIZE; i++) {
    pthread_create(&thread_pool[i], NULL, thread_handler, NULL);
  }

  /** inicializa os semaforos e o lock */
  sem_init(&queue_empty, 0, QUEUE_SIZE);
  sem_init(&queue_full, 0, 0);
  pthread_mutex_init(&queue_lock, 0);

  connection_queue = newQueue(QUEUE_SIZE);

  for (;;) {
    connection_fd = accept(listen_fd, NULL, NULL);

    // espera se a fila estiver vazia
    if (sem_trywait(&queue_empty) == 0) {
      int *p_conn = malloc(sizeof(*p_conn));
      *p_conn = connection_fd;
      pthread_mutex_lock(&queue_lock);
      { push(connection_queue, p_conn); }
      pthread_mutex_unlock(&queue_lock);
      sem_post(&queue_full);
    } else {
      // recusa conexões se a fila estiver cheia
      write(connection_fd, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 35);
      close(connection_fd);
    }
  }

  return 0;
}

void handle_connection(int connection_fd) {
  /** buffer para receber dados */
  char buffer[MAXLINE];
  memset(buffer, 0, MAXLINE);

  /** string para o caminho do arquivo */
  char path[MAXLINE];

  /** le os dados recebidos */
  int received = read(connection_fd, buffer, MAXLINE);

  if (received < 0) {
    fprintf(stderr, "Erro ao ler do socket.\n");
  } else if (received == 0) {
    fprintf(stderr, "Conexao fechada.\n");
  } else {
    /* printf("%s\n", buffer); // imprime o que foi recebido */

    /** faz um "parsing" do que foi recebido */
    char *method = strtok(buffer, " \t\r\n"); // metodo http
    char *uri = strtok(NULL, " \t");          // caminho do arquivo

    if (strncmp(method, "GET", 4) == 0) {
      /** se nenhum arquivo foi especificado, usa o index.html */
      if (strncmp(uri, "/", 2) == 0) {
        uri = "/index.html";
      }

      strcpy(path, ROOT);      /** pwd */
      strcat(path, "/public"); /** servir da pasta public */
      strcat(path, uri);       /** apenda o path recebido */

      /** tenta abrir o arquivo */
      FILE *file = fopen(path, "rb");

      if (file == NULL) {
        /** se o arquivo não existir, retorna um erro 404 */
        char *error_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(connection_fd, error_msg, strlen(error_msg));
      } else {
        /** se o arquivo existir, retorna o conteúdo do arquivo */
        char *ok_msg = "HTTP/1.1 200 OK\r\n\r\n";
        write(connection_fd, ok_msg, strlen(ok_msg));

        /** limpa o buffer antes de enviar o arquivo */
        memset(buffer, 0, MAXLINE);
        int bytes_read;
        while ((bytes_read = fread(buffer, 1, MAXLINE, file)) > 0) {
          write(connection_fd, buffer, bytes_read);
        }

        fclose(file);
      }
    } else {
      write(connection_fd, "HTTP/1.1 501 Not Implemented\r\n\r\n", 31);
    }
  }

  close(connection_fd);
}

void *thread_handler() {
  /** a thread deve rodar pra sempre */
  while (1) {
    int connection_fd;
    sem_wait(&queue_full);
    {
      pthread_mutex_lock(&queue_lock);
      {
        int *p_conn = dequeue(connection_queue);
        connection_fd = *p_conn;
        free(p_conn);
      }
      pthread_mutex_unlock(&queue_lock);
    }
    sem_post(&queue_empty);

    handle_connection(connection_fd);
  }
}
