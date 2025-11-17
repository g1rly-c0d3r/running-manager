#ifndef QUEUE
#define QUEUE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Queue {
  struct Node *front;
  struct Node *end;
};

struct List {
  struct ListNode *head;
};

typedef struct ListNode ListNode;
struct ListNode {
  uint8_t threads;
  char *name;
  pthread_t *thread;
  ListNode *next_node;
  ListNode *prev_node;
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
  node->threads_needed = 1;

  FILE *check_threads = fopen(script, "r");

  while (1) {
    if (fgets(buffer, buffsize, check_threads) == NULL) {
      break;
    };
    if (strncmp(buffer, "#RNMN threads = ", 16) == 0) {
      node->threads_needed = (uint8_t)strtoul(&buffer[16], NULL, 10);
      break;
    }
  }
  fclose(check_threads);

  node->prev_node = NULL;
  if (is_empty(queue)) {
    node->next_node = NULL;
    queue->front = node;
  } else {
    node->next_node->prev_node = queue->end;
    node->next_node = queue->end;
  }
  queue->end = node;
}

void dequeue(struct Queue *queue) {
  queue->front = queue->front->prev_node;
  free(queue->front->next_node);
  queue->front->next_node = NULL;
}

struct Queue *create_queue(void) {
  struct Queue *queue = malloc(sizeof(struct Queue));
  queue->front = NULL;
  queue->end = NULL;

  return queue;
}

struct List *create_list(void) {
  struct List *list = malloc(sizeof(struct List));

  list->head = NULL;

  return list;
}

void push(struct List *list, char *script, uint8_t threads,
          pthread_t *running_thread) {
  ListNode *node = malloc(sizeof(ListNode *));
  *node = (ListNode){.threads = threads,
                     .name = script,
                     .thread = running_thread,
                     .next_node = list->head,
                     .prev_node = NULL};

  if (list->head != NULL)
    list->head->prev_node = node;
  list->head = node;
}

void remove_node(struct List *list, char *name) {
  ListNode *traveler = list->head;

  while (traveler->next_node != NULL) {
    if (strcmp(traveler->name, name)) {
      traveler->prev_node->next_node = traveler->next_node;
      traveler->next_node->prev_node = traveler->prev_node;
      free(traveler);
      break;
    }
  }
}
#endif /* ifndef QUEUE                                                         \
#define QUEUE */
