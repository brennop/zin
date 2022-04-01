#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "queue.h"

/** porta para ouvir */
#define PORT 8080
/** tamanho do buffer para ler/responder conexões */
#define MAXLINE 1024
/** tamanho do backlog do socket */
#define MAX_CLIENTS 100
/** tamanho da thread pool */
#define POOL_SIZE 20
/** tamanho da fila de conexões */
#define QUEUE_SIZE 50

pthread_t thread_pool[POOL_SIZE];
queue_t connection_queue;
char *ROOT;

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
  // diretório atual
  ROOT = getenv("PWD");

  // inicializa a fila de conexões/clientes
  queue_init(&connection_queue, QUEUE_SIZE);

  // cria as threads da thread pool
  for (int i = 0; i < POOL_SIZE; i++) {
    pthread_create(&thread_pool[i], NULL, thread_handler, NULL);
  }

  int server_socket = setup_server(PORT);

  fd_set current_sockets, ready_sockets;

  FD_ZERO(&current_sockets);
  FD_SET(server_socket, &current_sockets);

  while (1) {
    /** copia os sockets */
    ready_sockets = current_sockets;

    if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
      fprintf(stderr, "Erro ao ler os socket.");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET(i, &ready_sockets)) {
        if (i == server_socket) {
          int connection_fd = accept(server_socket, NULL, NULL);
          FD_SET(connection_fd, &current_sockets);
        } else {
          int *connection_fd_p = malloc(sizeof(*connection_fd_p));
          *connection_fd_p = i;

          if (queue_trypush(&connection_queue, connection_fd_p) != 0) {
            // recusa conexões se a fila estiver cheia
            write(i, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 36);
            close(i);
          }
          FD_CLR(i, &current_sockets);
        }
      }
    }
  }

  return 0;
}

int setup_server(int port) {
  /** descritor do socket */
  int server_fd;
  struct sockaddr_in addr;

  /** cria um socket */
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  check(server_fd, "Erro ao criar o socket");

  /** inicializa informações de endereçamento */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY; /** endereço para escutar (any) */
  addr.sin_port = htons(port);       /** porta para escutar (8000) */

  /** associa a porta e começa a ouvir nela */
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

  /** string para o caminho do arquivo */
  char path[MAXLINE];

  /** le os dados recebidos */
  int received = read(connection_fd, buffer, MAXLINE);

  if (received < 0) {
    fprintf(stderr, "Erro ao ler do socket.\n");
  } else if (received == 0) {
    fprintf(stderr, "Conexao fechada.\n");
  } else {
    // faz um "parsing" do que foi recebido
    char *method = strtok(buffer, " \t\r\n"); // metodo http
    char *uri = strtok(NULL, " \t");          // caminho do arquivo

    // só sabemos lidar com o método GET
    if (strncmp(method, "GET", 4) == 0) {
      // se nenhum arquivo foi especificado, usa o index.html
      if (strncmp(uri, "/", 2) == 0) {
        uri = "/index.html";
      }

      strcpy(path, ROOT);      /** pwd */
      strcat(path, "/public"); /** servir da pasta public */
      strcat(path, uri);       /** apenda o path recebido */

      // tenta abrir o arquivo no caminho especificado
      FILE *file = fopen(path, "rb");

      if (file == NULL) {
        // se o arquivo não existir, retorna um erro 404
        char *error_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(connection_fd, error_msg, strlen(error_msg));
      } else {
        // se o arquivo existir, retorna o conteúdo do arquivo
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
    // respondemos "Not Implemented" pra qualquer outro método 
    } else {
      write(connection_fd, "HTTP/1.1 501 Not Implemented\r\n\r\n", 31);
    }
  }

  close(connection_fd);
}

void *thread_handler() {
  // a thread deve rodar pra sempre
  while (1) {
    int *connection_fd_p;
    queue_pop(&connection_queue, (void *)&connection_fd_p);
    int connection_fd = *connection_fd_p;
    free(connection_fd_p);

    handle_connection(connection_fd);
  }
}
