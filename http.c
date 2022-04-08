#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "queue.h"

/** porta para ouvir */
#define PORT 8080
/** tamanho do buffer para ler/responder conexões */
#define MAXLINE 2048
/** tamanho do backlog do socket */
#define MAX_CLIENTS 100
/** tamanho da thread pool */
#define POOL_SIZE 20
/** tamanho da fila de conexões */
#define QUEUE_SIZE 50

/** quanto tempo uma conexão dura */
#define TTL 60

#define MAX_USERS 50

pthread_t thread_pool[POOL_SIZE];
queue_t connection_queue;

stream_t message_stream;
queue_t publisher_queue;

int setup_server(int port);
void handle_connection(int connection_fd);
void *thread_handler();

void check(int value, char *message) {
  if (value == -1) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
  }
}

int main() {
  // inicializa a fila de conexões/clientes
  queue_init(&connection_queue, QUEUE_SIZE);

  // incializa a stream de mensagens
  stream_init(&message_stream);

  // cria as threads da thread pool
  for (int i = 0; i < POOL_SIZE; i++) {
    pthread_create(&thread_pool[i], NULL, thread_handler, NULL);
  }

  int server_socket = setup_server(PORT);

  while (1) {
    int connection_fd = accept(server_socket, NULL, NULL);

    if (queue_trypush(&connection_queue, connection_fd) != 0) {
      // recusa conexões se a fila estiver cheia
      write(connection_fd, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 36);
      close(connection_fd);
    }
  }

  return 0;
}

int setup_server(int port) {
  // descritor do socket
  int server_fd;
  struct sockaddr_in addr;

  // cria um socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  check(server_fd, "Erro ao criar o socket");

  // inicializa informações de endereçamento
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /** endereço para escutar (any) */
  addr.sin_port = htons(port);              /** porta para escutar (8000) */

  int enabled = 1; // permite pegar a porta após fechar
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

  // associa a porta e começa a ouvir nela
  int bind_result = bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
  check(bind_result, "Erro ao associar a porta.");

  int listen_result = listen(server_fd, MAX_CLIENTS);
  check(listen_result, "Erro ao tentar ouvir na porta.");

  printf("Esperando conexoes na porta %d\n", PORT);
  return server_fd;
}

void handle_connection(int connection_fd) {
  /** buffer para receber dados */
  char buffer[MAXLINE];
  memset(buffer, 0, MAXLINE);

  /** le os dados recebidos */
  int received = read(connection_fd, buffer, MAXLINE);

  if (received < 0) {
    fprintf(stderr, "Erro ao ler do socket.\n");
  } else if (received == 0) {
    fprintf(stderr, "Conexao fechada.\n");
  } else {
    // faz um "parsing" do que foi recebido
    char *method = strtok(buffer, " \t\r\n"); // metodo http
    char *path = strtok(NULL, " \t");         // caminho do arquivo

    if (strncmp(method, "GET", 4) == 0) {
      // se nenhuma sala foi especificada, enviamos o index.html
      if (strncmp(path, "/", 2) == 0) {
        // tenta abrir o arquivo no caminho especificado
        FILE *file = fopen("index.html", "rb");

        if (file == NULL) {
          // se o arquivo não existir, retorna um erro 404
          char *error_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
          write(connection_fd, error_msg, strlen(error_msg));
        } else {
          // se o arquivo existir, retorna o conteúdo do arquivo
          char *msg = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n\r\n";
          write(connection_fd, msg, strlen(msg));

          // limpa o buffer antes de enviar o arquivo
          memset(buffer, 0, MAXLINE);
          int bytes_read;
          while ((bytes_read = fread(buffer, 1, MAXLINE, file)) > 0) {
            write(connection_fd, buffer, bytes_read);
          }

          fclose(file);
        }
      } else {
        queue_t message_queue;
        queue_init(&message_queue, 24);
        int subscription = stream_subscribe(&message_stream, &message_queue);

        // iniciamos um sse
        char *msg = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n\r\n";
        write(connection_fd, msg, strlen(msg));

        // avisa o cliente para tentar se conectar a cada 5 segundos
        write(connection_fd, "retry: 5000\r\n\r\n", 16);

        time_t timeout = time(0) + 360;
        while (time(0) < timeout) {
          char *message;
          queue_pop(&message_queue, (void **)&message);

          write(connection_fd, "data: ", 6);
          write(connection_fd, message, strlen(message));
          write(connection_fd, "\r\n\r\n", 4);
        }

        stream_unsubscribe(&message_stream, subscription);
      }
    } else if (strncmp(method, "POST", 5) == 0) {
      char *payload;
      // consome os headers
      while (1) {
        // consome a chave
        payload = strtok(NULL, "\r\n: \t");
        if (!payload)
          break;

        payload = strtok(NULL, "\r\n");
        payload = payload + 1 + strlen(payload);
        if (payload[1] == '\r' && payload[2] == '\n')
          break;
      }

      payload = strtok(NULL, "\r\n");

      if (payload == NULL) {
        char *error_msg = "HTTP/1.1 400 Bad Request\r\n\r\n";
        write(connection_fd, error_msg, strlen(error_msg));
      } else {
        char *message = malloc((strlen(payload)) * sizeof(*message));
        strcat(message, payload);

        queue_push(&message_stream.publisher_queue, message);

        char *msg = "HTTP/1.1 201 Created\r\n\r\n";
        write(connection_fd, msg, strlen(msg));
      }
    } else {
      // respondemos "Not Implemented" pra qualquer outro método
      write(connection_fd, "HTTP/1.1 501 Not Implemented\r\n\r\n", 31);
    }
  }

  close(connection_fd);
}

void *thread_handler() {
  // a thread deve rodar pra sempre
  while (1) {
    int connection_fd;
    queue_pop(&connection_queue, &connection_fd);

    handle_connection(connection_fd);
  }
}

void *stream_replicate(void *arg) {
  stream_t *stream = (stream_t *)arg;
  while (1) {
    char *message;
    queue_pop(&stream->publisher_queue, (void **)&message);
    pthread_mutex_lock(&stream->mutex);
    {
      for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        queue_t *q = stream->subscriber_queues[i];
        if (q != NULL) {
          queue_push(stream->subscriber_queues[i], message);
        }
      }
    }
    pthread_mutex_unlock(&stream->mutex);
  }
}
