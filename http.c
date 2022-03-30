#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAXLINE 1024
#define MAX_CLIENTS 10

void *handle_connection(void *);
char *ROOT;

int main() {
  int listen_fd, connection_fd;
  struct sockaddr_in addr;

  /** caminho para servir os arquivos */
  ROOT = getenv("PWD");

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
  /** descritor da conexão */
  int connection_fd = *(int *)arg;

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
    printf("%s\n", buffer); /** imprime o que foi recebido */

    /** faz um "parsing" do que foi recebido */
    char *method = strtok(buffer, " \t\r\n"); // metodo http
    char *uri = strtok(NULL, " \t");          // caminho do arquivo
    char *prot = strtok(NULL, " \t\r\n");     // versão do protocolo

    if (strncmp(method, "GET", 4) == 0) {
      /** se nenhum arquivo foi especificado, usa o index.html */
      if (strncmp(uri, "/", 2) == 0) {
        uri = "/index.html";
      }

      strcpy(path, ROOT);      /** pwd */
      strcat(path, "/public"); /** servir da pasta public */
      strcat(path, uri);       /** apenda o path recebido */
      printf("file: %s\n", path);

      /** tenta abrir o arquivo */
      FILE *file = fopen(path, "rb");

      if (file == NULL) {
        printf("404: Arquivo nao encontrado.\n");
        /** se o arquivo não existir, retorna um erro 404 */
        char *error_msg = "HTTP/1.1 404 Not Found\r\n\r\n";
        write(connection_fd, error_msg, strlen(error_msg));
      } else {
        /** se o arquivo existir, retorna o conteúdo do arquivo */
        char *ok_msg = "HTTP/1.1 200 OK\r\n\r\n";
        write(connection_fd, ok_msg, strlen(ok_msg));

        /** limpa o buffer antes de enviar o arquivo */
        memset(buffer, 0, MAXLINE);
        /* while (fgets(buffer, MAXLINE, file) != NULL) { */
        /*   write(connection_fd, buffer, strlen(buffer)); */
        /* } */
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

  return NULL;
}
