#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Queue {
  struct Node *front;
  struct Node *end;
};

typedef struct Node Node;
struct Node {
  uint8_t threads_needed;
  char *script;
  struct Node *next_node;
  struct Node *prev_node;
};

bool is_empty(const struct Queue *queue) {
  return queue->end == NULL && queue->front == NULL;
}

void queue_sim(struct Queue *queue, char *script) {
  struct Node *node = malloc(sizeof(struct Node));
  const uint8_t buffsize = 100;
  char buffer[buffsize];

  node->script = script;

  FILE *check_threads = fopen(script, "r");

  while (1) {
    fgets(buffer, buffsize, check_threads);
    if (strncmp(buffer, "#RNMN threads = ", 17)) {
      node->threads_needed = (uint8_t)strtoul(&buffer[16], NULL, 10);
      break;
    }
  }
  fclose(check_threads);

  node->prev_node = NULL;
  if (is_empty(queue)) {
    node->next_node = NULL;
  } else {
    node->next_node = queue->end;
  }
  queue->end = node;
}

struct Queue *create_queue(void) {
  struct Queue *queue = malloc(sizeof(struct Queue));
  queue->front = NULL;
  queue->end = NULL;

  return queue;
}
