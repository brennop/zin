# Sincronização entre Processos

- Brenno Pereira Cordeiro
- 190127465

## Introdução

Muitos problemas mapeiam

Nosso servidor será projetado em cima do protocolo HTTP. O protocolo HTTP é o
principal protocolo em atuação na Web, e permite que clientes conectados 

## Problema

Um sistema de chat (mensagens instantâneas) via HTTP. Os usuário conseguem
acessar o sistema por meio de um navegador _Web_, como o _Google Chrome_, e são
capazes de enviar mensagens por meio de uma caixa de entrada de texto e enquanto
estão no _website_, irão receber mensagens enviadas pelos demais usuários.

Os usuários podem acessar o sistema concorrentemente para receber a página Web,
e por tanto, o serviço deve estar preparado para servir múltiplos usuários
concorrentemente. Os usuários também podem enviar mensagens concorrentemente, e
estarem prontos para receber mensagens ao mesmo tempo. O serviço deve considerar
todos esses casos e como atender uma quantidade desconhecidade de clientes.

## Solução

Nosso servidor HTTP deve atender múltiplos clientes sobre a rede. Como HTTP
funciona sobre o protocolo TCP, iremos usar a biblioteca `socket` para lidar com
as conexões. O código consiste em básicamente pedir para o sistema operacional
para nos associar a uma porta TCP, e chamar a função `accept` no _socket_ aberto
para esperar uma conexão e nos entregar um descritor, que aponta para a conexão
recebida.

Para lidar com essas conexões concorrentemente, iremos criar uma _thread pool_,
ou um grupo de threads, que lidam com as conexões a medida que chegam. 

### Fila Concorrente

Para resolver esse último problema, iremos usar uma fila concorrente. Uma fila
concorrente funciona assim como uma fila normal, porém suas operações utilizam
de locks e variáveis de condição para garantir que possa ser acessada por
múltiplas threads. 

Como referência para implementação e projeto da API, foi utilizado a _Thread
Safe FIFO bounded queue_ [1], disponível no projeto Apache Portable Runtime,
licensiado sobre a licensa Apache License.

A implementação apresentada nesse projeto difere da referência, removendo partes
que não foram utilizadas e adicionando uma funcionalidade para lidar com novos
problemas.

A fila implementada consiste basicamente de um array de ponteiros para guardar
os dados, um inteiro para guardar o próximo índice para inserção e outro para
guardar o próximo índice para remoção, alguns inteiros para lidar com os
limites. Além disso, nossa fila possui um _mutex_ e duas variáveis de condição,
para indicar se a fila deixou de estar vazia ou deixou de estar cheia.

### Operações

```c
void queue_push(queue_t *queue, void *element);
```

A operação _push_ tenta inserir um elemento na fila se há espaço, e se não há,
esperar a variável de condição _not_full_ ser sinalizada. Ao inserir, avisa
qualquer thread que possa estar esperando na variável de condição _not_empty_.

```c
int queue_trypush(queue_t *queue, void *element);
```

Alguma vezes, não precisamos esperar a fila liberar espaço, e queremos lidar com
isso imediatamente. Para isso, a função _trypush_ não entra em esperar se a fila
não possuir espaço, e retorna imediatamente com o valor -1.

```c
void queue_pop(queue_t *queue, void **element);
```

A operação _pop_ tenta remover um elemento da fila se estiver populada, e se
estiver vazia, espera na variável de condição _not_empty_. Se alguma operação de
_push_ ocorrer, será acordada para consumir o elemento inserido. Ao remover um
elemento, sinaliza qualquer thread que posso estar esperando na variável de
condição _not_full_.

[1]: https://apr.apache.org/docs/apr-util/0.9/group__APR__Util__FIFO.html

