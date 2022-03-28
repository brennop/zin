#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8000
#define MAXLINE 1024
#define MAX_CLIENTS 10

void *handle_connection(void *);

int main() {
  int listen_fd, connection_fd;
  struct sockaddr_in addr;

  /** cria um socket */
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  /** inicializa informações de endereçamento */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /** endereço para escutar (any)*/
  addr.sin_port = htons(PORT);              /** porta para escutar (8000) */

  bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
  listen(listen_fd, MAX_CLIENTS);
  printf("Esperando conexoes na porta %d\n", PORT);

  for (;;) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    connection_fd = accept(listen_fd, NULL, NULL);
    handle_connection(&connection_fd);

    close(connection_fd);
  }

  return 0;
}

void *handle_connection(void *arg) {
  int connection_fd = *(int *)arg;
  char buffer[MAXLINE];

  int received = read(connection_fd, buffer, MAXLINE);

  if (received < 0) {
    fprintf(stderr, "Erro ao ler do socket\n");
  } else if (received == 0) {
    fprintf(stderr, "Conexao fechada\n");
  } else {
    printf("%s\n", buffer);
  }

  char *method = strtok(buffer, " \t\r\n");
  char *uri = strtok(NULL, " \t");
  char *prot = strtok(NULL, " \t\r\n");

  fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);

  char *response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: 12\r\n"
                   "\r\n"
                   "Hello World!";

  write(connection_fd, response, strlen(response));

  return NULL;
}
